/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "rss_session.h"

#include <algorithm>

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVariantHash>

#include "base/asyncfilestorage.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"
#include "base/utils/sql.h"
#include "../rss_article.h"
#include "../rss_folder.h"
#include "../rss_item.h"
#include "rss_feedimpl.h"
#include "rss_parser.h"

const int MSECS_PER_MIN = 60000;

const QString CONF_FOLDER(QStringLiteral("rss"));
const QString DATA_FOLDER(QStringLiteral("rss/articles"));
const QString FEEDS_FILENAME(QStringLiteral("feeds.json"));

const QString DB_CONNECTION_NAME {QStringLiteral("rss")};
const QString DB_FILENAME {QStringLiteral("rss.db")};

const QString INVALID_NAME_CHAR_PATTERN {QStringLiteral(R"([/\\])")};

namespace
{
    namespace SQL = Utils::SQL;

    bool isValidName(const QString &name)
    {
        return !name.contains(QRegularExpression {INVALID_NAME_CHAR_PATTERN});
    }

    class ArticleSortAdaptor
    {
    public:
        ArticleSortAdaptor() = default;

        ArticleSortAdaptor(const RSS::Article *article)
            : m_isArticle {true}
            , m_ptr {article}
        {
        }

        ArticleSortAdaptor(const QVariantHash &dict)
            : m_isArticle {false}
            , m_ptr {&dict}
        {
        }

        bool isArticle() const
        {
            return m_isArticle;
        }

        const RSS::Article *asArticlePtr() const
        {
            return reinterpret_cast<const RSS::Article *>(m_ptr);
        }

        const QVariantHash &asDict() const
        {
            return *reinterpret_cast<const QVariantHash *>(m_ptr);
        }

        QDateTime getPubDate() const
        {
            return m_isArticle ? asArticlePtr()->date()
                             : asDict().value(RSS::Article::KeyDate).toDateTime();
        }

    private:
        bool m_isArticle = true;
        const void *m_ptr = nullptr;
    };
}

RSS::Private::Session::Session(int refreshInterval, int maxArticlesPerFeed, QObject *parent)
    : QObject {parent}
    , m_workingThread {new QThread {this}}
{
    setMaxArticlesPerFeed(maxArticlesPerFeed);

    initializeDatabase();

    m_itemsById.insert(0, new Folder {0}); // root folder
    m_itemsByPath.insert("", m_itemsById.value(0));

    m_parser = new Parser;
    m_parser->moveToThread(m_workingThread);
    connect(m_workingThread, &QThread::finished, m_parser, &Parser::deleteLater);
    connect(m_parser, &Parser::finished, this, &RSS::Private::Session::handleFeedParsingFinished);

    m_workingThread->start();
    load();

    connect(&m_refreshTimer, &QTimer::timeout, this, &RSS::Private::Session::refreshAll);
    setRefreshInterval(refreshInterval);
}

RSS::Private::Session::~Session()
{
    qDebug() << "Deleting RSS Session...";

    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);

    m_workingThread->quit();
    m_workingThread->wait();

    delete m_itemsById[0]; // deleting root folder

    qDebug() << "RSS Session deleted.";
}

bool RSS::Private::Session::addFolder(const QString &path, QString *error)
{
    Folder *destFolder = prepareItemDest(path, error);
    if (!destFolder)
        return false;

    const QString name = Item::relativeName(path);

    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
    query.bindValue(":parentId", destFolder->id());
    query.bindValue(":name", name);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    const qint64 folderId = query.lastInsertId().value<qint64>();
    addItem(new Folder {folderId, path}, destFolder);
    return true;
}

bool RSS::Private::Session::addFeed(const QString &url, const QString &path, QString *error)
{
    Folder *destFolder = prepareItemDest(path, error);
    if (!destFolder)
        return false;

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    if (!db.transaction()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    const QString name = Item::relativeName(path);

    QSqlQuery query {db};
    query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
    query.bindValue(":parentId", destFolder->id());
    query.bindValue(":name", name);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        db.rollback();
        return false;
    }

    const qint64 itemId = query.lastInsertId().value<qint64>();

    query.prepare("INSERT INTO feed (id, url) VALUES(:id, :url);");
    query.bindValue(":id", itemId);
    query.bindValue(":url", url);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    addItem(new FeedImpl {itemId, url, path, maxArticlesPerFeed()}, destFolder);
    if (m_refreshInterval != 0)
        refreshItem(*feedByURL(url));
    return true;
}

void RSS::Private::Session::moveItem(Item &item, Folder &destFolder, const QString &name)
{
    if (item.id() == 0)
        throw RuntimeError {tr("Cannot move root folder.")};

    const QString destName = (name.isEmpty() ? item.name() : name);

    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("UPDATE item SET name = :name, parentId = :parentId WHERE id = :id;");
    query.bindValue(":id", item.id());
    query.bindValue(":name", destName);
    query.bindValue(":parentId", destFolder.id());
    if (!query.exec())
        throw RuntimeError {query.lastError().text()};

    RSS::Folder &srcFolder = *item.parent();
    srcFolder.removeItem(&item);
    destFolder.addItem(&item);
    const QString destPath = Item::joinPath(destFolder.path(), destName);
    m_itemsByPath.insert(destPath, m_itemsByPath.take(item.path()));
    item.setPath(destPath);
}

void RSS::Private::Session::removeItem(Item &item)
{
    if (item.id() == 0)
        throw RuntimeError {tr("Cannot remove root folder.")};

    emit itemAboutToBeRemoved(&item);

    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("DELETE FROM item WHERE id = :id;");
    query.bindValue(":id", item.id());
    if (!query.exec())
        throw RuntimeError {query.lastError().text()};

    cleanupItemData(item);

    item.parent()->removeItem(&item);
    delete &item;
}

bool RSS::Private::Session::moveItem(const QString &itemPath, const QString &destPath, QString *error)
{
    if (itemPath.isEmpty()) {
        if (error)
            *error = tr("Cannot move root folder.");
        return false;
    }

    auto *item = m_itemsByPath.value(itemPath);
    if (!item) {
        if (error)
            *error = tr("Item doesn't exist: %1.").arg(itemPath);
        return false;
    }

    Folder *destFolder = prepareItemDest(destPath, error);
    if (!destFolder)
        return false;

    try {
        moveItem(*item, *destFolder);
    }
    catch (RuntimeError &err) {
        if (error)
            *error = err.message();
        return false;
    }

    return true;
}

bool RSS::Private::Session::removeItem(const QString &itemPath, QString *error)
{
    if (itemPath.isEmpty()) {
        if (error)
            *error = tr("Cannot delete root folder.");
        return false;
    }

    auto *item = m_itemsByPath.value(itemPath);
    if (!item) {
        if (error)
            *error = tr("Item doesn't exist: %1.").arg(itemPath);
        return false;
    }

    emit itemAboutToBeRemoved(item);

    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("DELETE FROM item WHERE id = :id;");
    query.bindValue(":id", item->id());
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    cleanupItemData(*item);

    auto *folder = static_cast<Folder *>(m_itemsByPath.value(Item::parentPath(item->path())));
    folder->removeItem(item);
    delete item;
    return true;
}

void RSS::Private::Session::refreshItem(qint64 itemId)
{
    auto *item = m_itemsById.value(itemId);
    if (item)
        refreshItem(*item);
}

QList<RSS::Item *> RSS::Private::Session::items() const
{
    return m_itemsByPath.values();
}

RSS::Item *RSS::Private::Session::itemByID(const qint64 id) const
{
    for (auto *item : qAsConst(m_itemsByPath)) {
        if (item->id() == id)
            return item;
    }

    return nullptr;
}

RSS::Item *RSS::Private::Session::itemByPath(const QString &path) const
{
    return m_itemsByPath.value(path);
}

void RSS::Private::Session::load()
{
    loadFolder(0, rootFolder());
}

void RSS::Private::Session::loadFolder(const qint64 folderId, Folder *folder)
{
    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("SELECT item.id, item.name, feed.url FROM item LEFT JOIN feed ON (feed.id = item.id)"
                  " WHERE item.parentId = :parentId");
    query.bindValue(":parentId", folderId);
    if (!query.exec()) {
        LogMsg(tr("Couldn't load RSS folder #%1: %2").arg(folderId).arg(query.lastError().text()), Log::CRITICAL);
        return;
    }

    while (query.next()) {
        const qint64 id = query.value(0).value<qint64>();
        const QString name = query.value(1).toString();
        const QString url = query.value(2).toString();

        if (url.isNull()) {
            auto *subfolder = new Folder {id, Item::joinPath(folder->path(), name)};
            addItem(subfolder, folder);

            loadFolder(id, subfolder);
        }
        else {
            auto *feed = new FeedImpl {id, url, Item::joinPath(folder->path(), name), maxArticlesPerFeed()};
            loadFeedArticles(id, *feed);

            addItem(feed, folder);
        }
    }
}

void RSS::Private::Session::storeFeed(FeedImpl &feed)
{
    if (!feed.isDirty()) return;

    qDebug() << "Storing RSS Feed" << feed.url();

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    if (!db.transaction())
        throw RuntimeError(db.lastError().text());

    QSqlQuery query {db};

    query.prepare("DELETE FROM article WHERE feedId = :feedId;");
    query.bindValue(":feedId", feed.id());
    if (!query.exec()) {
        db.rollback();
        throw RuntimeError {query.lastError().text()};
    }

    query.prepare("INSERT INTO article (feedId, localId, date, title, author, description, torrentURL, link, isRead)"
                  "VALUES (:feedId, :localId, :date, :title, :author, :description, :torrentURL, :link, :isRead);");
    query.bindValue(":feedId", feed.id());
    for (Article *article : asConst(feed.articles())) {
        query.bindValue(":localId", article->localId());
        query.bindValue(":date", article->date().toString(Qt::ISODate));
        query.bindValue(":title", article->title());
        query.bindValue(":author", article->author());
        query.bindValue(":description", article->description());
        query.bindValue(":torrentURL", article->torrentUrl());
        query.bindValue(":link", article->link());
        query.bindValue(":isRead", article->isRead());
        if (!query.exec()) {
            db.rollback();
            throw RuntimeError {query.lastError().text()};
        }
    }

    if (!db.commit())
        throw RuntimeError {db.lastError().text()};

    feed.setDirty(false);
}

RSS::Folder *RSS::Private::Session::prepareItemDest(const QString &path, QString *error)
{
    if (!Item::isValidPath(path)) {
        if (error)
            *error = tr("Incorrect RSS Item path: %1.").arg(path);
        return nullptr;
    }

    if (m_itemsByPath.contains(path)) {
        if (error)
            *error = tr("RSS item with given path already exists: %1.").arg(path);
        return nullptr;
    }

    const QString destFolderPath = Item::parentPath(path);
    auto destFolder = qobject_cast<Folder *>(m_itemsByPath.value(destFolderPath));
    if (!destFolder) {
        if (error)
            *error = tr("Parent folder doesn't exist: %1.").arg(destFolderPath);
        return nullptr;
    }

    return destFolder;
}

void RSS::Private::Session::addItem(Item *item, Folder *destFolder)
{
    if (auto *feed = qobject_cast<FeedImpl *>(item)) {
        m_feedsByURL[feed->url()] = feed;
        Net::DownloadManager::instance()->registerSequentialService(Net::ServiceID::fromURL(feed->url()));
    }

    connect(item, &Item::pathChanged, this, &RSS::Private::Session::itemPathChanged);
    m_itemsById[item->id()] = item;
    m_itemsByPath[item->path()] = item;
    destFolder->addItem(item);
    emit itemAdded(item);
}

void RSS::Private::Session::cleanupItemData(const Item &item)
{
    m_itemsByPath.remove(item.path());
    m_itemsById.remove(item.id());

    auto *feed = qobject_cast<const FeedImpl *>(&item);
    if (feed) {
        m_feedsByURL.remove(feed->url());
        return;
    }

    auto *folder = static_cast<const Folder *>(&item);
    for (auto *item : asConst(folder->items()))
        cleanupItemData(*item);
}

void RSS::Private::Session::refreshItem(Item &item)
{
    auto *feed = qobject_cast<FeedImpl *>(&item);
    if (feed) {
        if (feed->isLoading()) return;

        Net::DownloadManager::instance()->download(feed->url(), this, &RSS::Private::Session::handleFeedDownloadFinished);

        feed->setLoading(true);
        emit feedStateChanged(feed);
    }
    else {
        auto *folder = static_cast<Folder *>(&item);
        for (auto *item : asConst(folder->items()))
            refreshItem(*item);
    }
}

void RSS::Private::Session::loadFeedArticles(const qint64 feedId, FeedImpl &feed)
{
    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("SELECT * FROM article WHERE feedId = :feedId ORDER BY date;");
    query.bindValue(":feedId", feedId);
    if (!query.exec()) {
        LogMsg(tr("Couldn't load RSS feed #%1: %2").arg(feedId).arg(query.lastError().text()), Log::CRITICAL);
        return;
    }

    while (query.next()) {
        const QSqlRecord rec = query.record();
        QVariantHash dict;
        for (int i = 0; i < rec.count(); ++i)
            dict[rec.fieldName(i)] = rec.value(i);
        // Date are stored as string so we need to convert it
        dict[Article::KeyDate] = QDateTime::fromString(dict.value(Article::KeyDate).toString(), Qt::ISODate);

        try {
            auto *article = new Article {&feed, dict};
            if (!feed.addArticle(article))
                delete article;
        }
        catch (const std::runtime_error&) {}
    }
}

void RSS::Private::Session::handleFeedDownloadFinished(const Net::DownloadResult &result)
{
    auto *feed = m_feedsByURL.value(result.url);
    if (!feed) return;

    if (result.status == Net::DownloadStatus::Success) {
        qDebug() << "Successfully downloaded RSS feed at" << result.url;
        // Parse the download RSS
        QMetaObject::invokeMethod(m_parser, "parse", Q_ARG(QString, result.url)
                                  , Q_ARG(QByteArray, result.data), Q_ARG(QString, feed->lastBuildDate()));
    }
    else {
        feed->setLoading(false);
        feed->setHasError(true);

        LogMsg(tr("Failed to download RSS feed at '%1'. Reason: %2")
               .arg(result.url, result.errorString), Log::WARNING);

        emit feedStateChanged(feed);
    }
}

void RSS::Private::Session::handleFeedParsingFinished(const ParsingResult &result)
{
    FeedImpl *feed = m_feedsByURL.value(result.url);
    if (!feed) return;

    feed->setHasError(!result.error.isEmpty());

    if (!result.title.isEmpty()) {
        feed->setTitle(result.title);
        if (feed->name().startsWith('@')) {
            // Now we have something better than an auto generated name.
            // Trying to rename feed...
            try {
                renameItem(*feed, generateFeedName(feed->title(), *feed->parent()));
            }
            catch (RuntimeError &) {}
        }
    }

    if (!result.lastBuildDate.isEmpty())
        feed->setLastBuildDate(result.lastBuildDate);

    // For some reason, the RSS feed may contain malformed XML data and it may not be
    // successfully parsed by the XML parser. We are still trying to load as many articles
    // as possible until we encounter corrupted data. So we can have some articles here
    // even in case of parsing error.
    const int newArticlesCount = updateFeedArticles(*feed, result.articles);

    storeFeed(*feed);

    if (feed->hasError()) {
        LogMsg(tr("Failed to parse RSS feed at '%1'. Reason: %2").arg(feed->url(), result.error)
               , Log::WARNING);
    }
    LogMsg(tr("RSS feed at '%1' updated. Added %2 new articles.")
           .arg(feed->url(), QString::number(newArticlesCount)));

    feed->setLoading(false);
    emit feedStateChanged(feed);
}

QString RSS::Private::Session::generateFeedName(const QString &baseName, Folder &destFolder)
{
    QString name = baseName;
    int counter = 0;
    while (itemByPath(Item::joinPath(destFolder.path(), name)))
        name = baseName + ' ' + QString::number(++counter);

    return name;
}

int RSS::Private::Session::updateFeedArticles(RSS::Private::FeedImpl &feed, const QList<QVariantHash> &loadedArticles)
{
    if (loadedArticles.empty())
        return 0;

    QDateTime dummyPubDate {QDateTime::currentDateTime()};
    QVector<QVariantHash> newArticles;
    newArticles.reserve(loadedArticles.size());
    for (QVariantHash article : loadedArticles) {
        QVariant &torrentURL = article[Article::KeyTorrentURL];
        if (torrentURL.toString().isEmpty())
            torrentURL = article[Article::KeyLink];

        // if item does not have a guid, fall back to some other identifier
        QVariant &localId = article[Article::KeyLocalId];
        if (localId.toString().isEmpty())
            localId = article.value(Article::KeyTorrentURL);
        if (localId.toString().isEmpty())
            localId = article.value(Article::KeyTitle);

        if (localId.toString().isEmpty())
            continue;

        // If article has no publication date we use feed update time as a fallback.
        // To prevent processing of "out-of-limit" articles we must not assign dates
        // that are earlier than the dates of existing articles.
        const Article *existingArticle = feed.articleByGUID(localId.toString());
        if (existingArticle) {
            dummyPubDate = existingArticle->date().addMSecs(-1);
            continue;
        }

        QVariant &articleDate = article[Article::KeyDate];
        // if article has no publication date we use feed update time as a fallback
        if (!articleDate.toDateTime().isValid())
            articleDate = dummyPubDate;

        newArticles.append(article);
    }

    if (newArticles.empty())
        return 0;

    QVector<ArticleSortAdaptor> sortData;
    const QList<Article *> existingArticles = feed.articles();
    sortData.reserve(existingArticles.size() + newArticles.size());
    std::transform(existingArticles.begin(), existingArticles.end(), std::back_inserter(sortData)
                   , [](const Article *article)
    {
        return ArticleSortAdaptor {article};
    });
    std::transform(newArticles.begin(), newArticles.end(), std::back_inserter(sortData)
                   , [](const QVariantHash &article)
    {
        return ArticleSortAdaptor {article};
    });

    // Sort article list in reverse chronological order
    std::sort(sortData.begin(), sortData.end()
              , [](const ArticleSortAdaptor &a1, const ArticleSortAdaptor &a2)
    {
        return (a1.getPubDate() > a2.getPubDate());
    });

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    if (!db.transaction())
        throw RuntimeError(db.lastError().text());

    QSqlQuery query {db};

    const int outOfLimitCount = std::max(0, (sortData.size() - maxArticlesPerFeed()));
    if (outOfLimitCount > 0) {
        query.prepare("DELETE FROM article WHERE feedId = :feedId AND localId = :localId;");
        query.bindValue(":feedId", feed.id());
        std::for_each(sortData.crbegin(), (sortData.crbegin() + outOfLimitCount)
                      , [&](const ArticleSortAdaptor &a)
        {
            if (!a.isArticle()) return;

            query.bindValue(":localId", a.asArticlePtr()->localId());
            if (!query.exec()) {
                db.rollback();
                throw RuntimeError {query.lastError().text()};
            }
        });
        sortData.resize(maxArticlesPerFeed());
    }

    query.prepare("INSERT INTO article (feedId, localId, date, title, author, description, torrentURL, link, isRead)"
                  "VALUES (:feedId, :localId, :date, :title, :author, :description, :torrentURL, :link, :isRead);");
    query.bindValue(":feedId", feed.id());
    int newArticlesCount = 0;
    std::for_each((sortData.crbegin() + outOfLimitCount), sortData.crend()
                  , [&](const ArticleSortAdaptor &a)
    {
        if (a.isArticle()) return;

        auto *article = new Article {&feed, a.asDict()};

        query.bindValue(":localId", article->localId());
        query.bindValue(":date", article->date().toString(Qt::ISODate));
        query.bindValue(":title", article->title());
        query.bindValue(":author", article->author());
        query.bindValue(":description", article->description());
        query.bindValue(":torrentURL", article->torrentUrl());
        query.bindValue(":link", article->link());
        query.bindValue(":isRead", article->isRead());
        if (!query.exec()) {
            db.rollback();
            throw RuntimeError {query.lastError().text()};
        }

        feed.addArticle(article);
        ++newArticlesCount;
    });

    if (!db.commit())
        throw RuntimeError {db.lastError().text()};

    return newArticlesCount;
}

RSS::Folder *RSS::Private::Session::rootFolder() const
{
    return static_cast<Folder *>(m_itemsById.value(0));
}

QVector<RSS::Feed *> RSS::Private::Session::feeds() const
{
    QVector<RSS::Feed *> result {m_feedsByURL.size()};
    std::transform(m_feedsByURL.begin(), m_feedsByURL.end(), result.begin()
                   , [](FeedImpl *feedImpl)
    {
        return static_cast<RSS::Feed *>(feedImpl);
    });
    return result;
}

RSS::Feed *RSS::Private::Session::feedByURL(const QString &url) const
{
    return m_feedsByURL.value(url);
}

int RSS::Private::Session::refreshInterval() const
{
    return m_refreshInterval;
}

void RSS::Private::Session::setRefreshInterval(int refreshInterval)
{
    m_refreshInterval = qMax(refreshInterval, 0);
    if (m_refreshInterval != 0) {
        if (!m_refreshTimer.isActive())
            refreshAll();
        m_refreshTimer.start(m_refreshInterval * MSECS_PER_MIN);
    }
    else {
        m_refreshTimer.stop();
    }
}

RSS::Folder *RSS::Private::Session::addFolder(const QString &name, Folder &destFolder)
{
    if (!isValidName(name))
        throw RuntimeError {tr("Invalid name.")};

    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
    query.bindValue(":parentId", destFolder.id());
    query.bindValue(":name", name);
    if (!query.exec())
        throw RuntimeError {query.lastError().text()};

    const qint64 folderId = query.lastInsertId().value<qint64>();
    Folder *folder = new Folder {folderId, Item::joinPath(destFolder.path(), name)};
    addItem(folder, &destFolder);

    return folder;
}

RSS::Feed *RSS::Private::Session::addFeed(const QString &url, const QString &name, Folder &destFolder)
{
    if (!name.isEmpty() && !isValidName(name))
        throw RuntimeError {tr("Invalid name.")};

    const QString feedName = (name.isEmpty() ? generateFeedName('@' + QUrl(url).host(), destFolder) : name);

    auto db = QSqlDatabase::database(DB_CONNECTION_NAME);
    if (!db.transaction())
        throw RuntimeError {db.lastError().text()};

    QSqlQuery query {db};
    query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
    query.bindValue(":parentId", destFolder.id());
    query.bindValue(":name", feedName);
    if (!query.exec()) {
        db.rollback();
        throw RuntimeError {query.lastError().text()};
    }

    const qint64 itemId = query.lastInsertId().value<qint64>();

    query.prepare("INSERT INTO feed (id, url) VALUES(:id, :url);");
    query.bindValue(":id", itemId);
    query.bindValue(":url", url);
    if (!query.exec()) {
        db.rollback();
        throw RuntimeError {query.lastError().text()};
    }

    if (!db.commit())
        throw RuntimeError {db.lastError().text()};

    Feed *feed = new FeedImpl {itemId, url, Item::joinPath(destFolder.path(), feedName), maxArticlesPerFeed()};
    addItem(feed, &destFolder);
    if (m_refreshInterval != 0)
        refreshItem(*feed);

    return feed;
}

void RSS::Private::Session::renameItem(Item &item, const QString &name)
{
    if (item.id() == 0)
        throw RuntimeError {tr("Cannot rename root folder.")};

    moveItem(item, *item.parent(), name);
}

void RSS::Private::Session::initializeDatabase()
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), DB_CONNECTION_NAME);
    db.setDatabaseName(Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + DB_FILENAME));
    if (!db.open())
        throw RuntimeError(db.lastError().text());

    QSqlQuery query {db};
    if (!query.exec("PRAGMA foreign_keys = ON;"))
        throw RuntimeError(query.lastError().text());

    if (db.tables().toSet().contains({"item", "feed", "article"}))
        return;

    if (!db.transaction())
        throw RuntimeError(db.lastError().text());

    try {
        bool ok = query.exec(SQL::createTable("item")
                             .column("id", "INTEGER PRIMARY KEY")
                             .column("parentId", "INTEGER")
                             .column("name", "TEXT NOT NULL")
                             .unique({"parentId", "name"})
                             .foreignKey({"parentId"}, "item", {"id"}, "ON UPDATE CASCADE ON DELETE CASCADE")
                             .getQuery());
        if (!ok)
            throw RuntimeError(query.lastError().text());

        ok = query.exec(R"(INSERT INTO item (id, parentId, name) VALUES (0, NULL, "");)");
        if (!ok)
            throw RuntimeError(query.lastError().text());

        ok = query.exec(R"(
            CREATE TABLE feed (
                id INTEGER PRIMARY KEY REFERENCES item(id) ON UPDATE CASCADE ON DELETE CASCADE,
                url TEXT UNIQUE NOT NULL
            );
        )");
        if (!ok)
            throw RuntimeError(query.lastError().text());

        ok = query.exec(R"(
            CREATE TABLE article (
                id INTEGER PRIMARY KEY,
                feedId INTEGER NOT NULL REFERENCES feed(id) ON UPDATE CASCADE ON DELETE CASCADE,
                localId TEXT NOT NULL,
                date TEXT NOT NULL,
                title TEXT,
                author TEXT,
                description TEXT,
                torrentURL TEXT,
                link TEXT,
                isRead BOOLEAN NOT NULL DEFAULT 0 CHECK(isRead IN (0, 1)),
                UNIQUE(feedId, localId)
            );
        )");
        if (!ok)
            throw RuntimeError(query.lastError().text());

        migrate();

        ok = db.commit();
        if (!ok)
            throw RuntimeError(db.lastError().text());
    }
    catch (const RuntimeError &err) {
        db.rollback();
        throw err;
    }
}

void RSS::Private::Session::migrate()
{
    QFile itemsFile {QDir(Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Config) + CONF_FOLDER))
                .absoluteFilePath(FEEDS_FILENAME)};
    if (!itemsFile.exists()) {
        migrateFromLegacyData();
        return;
    }

    if (!itemsFile.open(QFile::ReadOnly)) {
        Logger::instance()->addMessage(
                    QString("Couldn't read RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName(), itemsFile.errorString()), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(itemsFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        Logger::instance()->addMessage(
                    QString("Couldn't parse RSS Session data from %1. Error: %2")
                    .arg(itemsFile.fileName(), jsonError.errorString()), Log::WARNING);
        return;
    }

    if (!jsonDoc.isObject()) {
        Logger::instance()->addMessage(
                    QString("Couldn't load RSS Session data from %1. Invalid data format.")
                    .arg(itemsFile.fileName()), Log::WARNING);
        return;
    }

    migrateFolder(jsonDoc.object(), 0);
}

void RSS::Private::Session::migrateFolder(const QJsonObject &jsonObj, const qint64 folderId)
{
    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};

    for (const QString &key : asConst(jsonObj.keys())) {
        const QJsonValue val {jsonObj[key]};
        const QString &name = key;

        query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
        query.bindValue(":parentId", folderId);
        query.bindValue(":name", name);
        if (!query.exec())
            throw RuntimeError(query.lastError().text());

        const qint64 itemId = query.lastInsertId().value<qint64>();

        if (val.isString() || (val.isObject() && val.toObject().contains("url"))) {
            QString url;
            QUuid uid;

            if (val.isString()) {
                // previous format (reduced form) doesn't contain UID
                url = val.toString();
                if (url.isEmpty())
                    url = key;

                uid = QUuid::createUuid();
            }
            else if (val.isObject()) {
                const QJsonObject valObj {val.toObject()};

                uid  = QUuid {valObj["uid"].toString()};
                if (uid.isNull())
                    uid = QUuid::createUuid();

                url = valObj["url"].toString();
            }

            query.prepare("INSERT INTO feed (id, url) VALUES(:id, :url);");
            query.bindValue(":id", itemId);
            query.bindValue(":url", url);
            if (!query.exec())
                throw RuntimeError(query.lastError().text());

            migrateFeedArticles(itemId, url, uid);
        }
        else if (val.isObject()) {
            migrateFolder(val.toObject(), itemId);
        }
    }
}

void RSS::Private::Session::migrateFromLegacyData()
{
    const QStringList legacyFeedPaths = SettingsStorage::instance()->loadValue("Rss/streamList").toStringList();
    const QStringList feedAliases = SettingsStorage::instance()->loadValue("Rss/streamAlias").toStringList();
    if (legacyFeedPaths.size() != feedAliases.size()) {
        Logger::instance()->addMessage("Corrupted RSS list, not loading it.", Log::WARNING);
        return;
    }

    int i = 0;
    QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
    for (QString legacyPath : legacyFeedPaths) {
        if (Item::PathSeparator == QString(legacyPath[0]))
            legacyPath.remove(0, 1);
        const QString feedUrl = Item::relativeName(legacyPath);

        qint64 parentId = 0;
        for (const QString &itemPath : Item::expandPath(legacyPath)) {
            const QString itemName = Item::relativeName(itemPath);

            query.prepare("SELECT id FROM item WHERE parentId = :parentId AND name = :name;");
            query.bindValue(":parentId", parentId);
            query.bindValue(":name", itemName);
            if (!query.exec())
                throw RuntimeError(query.lastError().text());

            if (query.first()) {
                parentId = query.value(0).value<qint64>();
                continue;
            }

            query.prepare("INSERT INTO item (name, parentId) VALUES(:name, :parentId);");
            query.bindValue(":parentId", parentId);
            query.bindValue(":name", itemName);
            if (!query.exec())
                throw RuntimeError(query.lastError().text());

            parentId = query.lastInsertId().value<qint64>();
        }

        const qint64 itemId = parentId;

        query.prepare("INSERT INTO feed (id, url) VALUES(:id, :url);");
        query.bindValue(":id", itemId);
        query.bindValue(":url", feedUrl);
        if (!query.exec())
            throw RuntimeError(query.lastError().text());

        migrateFeedArticlesLegacy(itemId, feedUrl);
        ++i;
    }
}

void RSS::Private::Session::migrateFeedArticles(qint64 feedId, const QString &url, const QUuid &uid)
{
    // Move to current (since v4.1.2) file naming scheme
    const QString legacyFilename {Utils::Fs::toValidFileSystemName(url, false, QLatin1String("_"))
                + QLatin1String(".json")};
    const QString currentFileName = QString::fromLatin1(uid.toRfc4122().toHex()) + QLatin1String(".json");
    const QDir storageDir {Utils::Fs::expandPathAbs(specialFolderLocation(SpecialFolder::Data) + DATA_FOLDER)};
    if (!QFile::exists(storageDir.absoluteFilePath(currentFileName)))
        QFile::rename(storageDir.absoluteFilePath(legacyFilename), storageDir.absoluteFilePath(currentFileName));

    QFile file {storageDir.absoluteFilePath(currentFileName)};

    if (!file.exists()) {
        migrateFeedArticlesLegacy(feedId, url);
        return;
    }

    if (!file.open(QFile::ReadOnly)) {
        LogMsg(tr("Couldn't read RSS articles from %1. Error: %2").arg(currentFileName, file.errorString())
               , Log::WARNING);
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        LogMsg(tr("Couldn't parse RSS articles data. Error: %1").arg(jsonError.errorString())
               , Log::WARNING);
        return;
    }

    if (!jsonDoc.isArray()) {
        LogMsg(tr("Couldn't load RSS articles data. Invalid data format."), Log::WARNING);
        return;
    }

    const QJsonArray jsonArr = jsonDoc.array();
    int i = -1;
    for (const QJsonValue &jsonVal : jsonArr) {
        ++i;
        if (!jsonVal.isObject()) {
            LogMsg(tr("Couldn't load RSS article '%1#%2'. Invalid data format.").arg(url).arg(i)
                   , Log::WARNING);
            continue;
        }

        const auto articleObj = jsonVal.toObject();
        QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
        query.prepare("INSERT INTO article (feedId, localId, date, title, author, description, torrentURL, link, isRead)"
                      "VALUES (:feedId, :localId, :date, :title, :author, :description, :torrentURL, :link, :isRead);");
        query.bindValue(":feedId", feedId);
        query.bindValue(":localId", articleObj[QLatin1String("id")].toString());
        query.bindValue(":date", QDateTime::fromString(articleObj[Article::KeyDate].toString(), Qt::RFC2822Date).toString(Qt::ISODate));
        query.bindValue(":title", articleObj[Article::KeyTitle].toString());
        query.bindValue(":author", articleObj[Article::KeyAuthor].toString());
        query.bindValue(":description", articleObj[Article::KeyDescription].toString());
        query.bindValue(":torrentURL", articleObj[Article::KeyTorrentURL].toString());
        query.bindValue(":link", articleObj[Article::KeyLink].toString());
        query.bindValue(":isRead", articleObj[Article::KeyIsRead].toBool());
        if (!query.exec())
            throw RuntimeError(query.lastError().text());
    }
}

void RSS::Private::Session::migrateFeedArticlesLegacy(qint64 feedId, const QString &url)
{
    SettingsPtr qBTRSSFeeds = Profile::instance().applicationSettings(QStringLiteral("qBittorrent-rss-feeds"));
    const QVariantHash allOldItems = qBTRSSFeeds->value("old_items").toHash();

    for (const QVariant &var : asConst(allOldItems.value(url).toList())) {
        auto hash = var.toHash();
        // update legacy keys
        hash[Article::KeyLocalId] = hash.take(QLatin1String("id"));
        hash[Article::KeyLink] = hash.take(QLatin1String("news_link"));
        hash[Article::KeyTorrentURL] = hash.take(QLatin1String("torrent_url"));
        hash[Article::KeyIsRead] = hash.take(QLatin1String("read"));

        QSqlQuery query {QSqlDatabase::database(DB_CONNECTION_NAME)};
        query.prepare("INSERT INTO article (feedId, localId, date, title, author, description, torrentURL, link, isRead)"
                      "VALUES (:feedId, :localId, :date, :title, :author, :description, :torrentURL, :link, :isRead);");
        query.bindValue(":feedId", feedId);
        query.bindValue(":localId", hash[Article::KeyLocalId].toString());
        query.bindValue(":date", hash[Article::KeyDate].toDateTime().toString(Qt::ISODate));
        query.bindValue(":title", hash[Article::KeyTitle].toString());
        query.bindValue(":author", hash[Article::KeyAuthor].toString());
        query.bindValue(":description", hash[Article::KeyDescription].toString());
        query.bindValue(":torrentURL", hash[Article::KeyTorrentURL].toString());
        query.bindValue(":link", hash[Article::KeyLink].toString());
        query.bindValue(":isRead", hash[Article::KeyIsRead].toBool());
        if (!query.exec())
            throw RuntimeError(query.lastError().text());
    }
}

int RSS::Private::Session::maxArticlesPerFeed() const
{
    return m_maxArticlesPerFeed;
}

void RSS::Private::Session::setMaxArticlesPerFeed(int n)
{
    m_maxArticlesPerFeed = qMax(n, 1);
    for (auto *feed : qAsConst(m_feedsByURL))
        feed->setMaxArticles(n);
}

void RSS::Private::Session::refreshAll()
{
    // NOTE: Should we allow manually refreshing for disabled session?
    for (auto *feed : qAsConst(m_feedsByURL))
        refreshItem(*feed);
}

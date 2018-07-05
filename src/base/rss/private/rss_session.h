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

#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>

class QThread;

namespace Net
{
    struct DownloadResult;
}

namespace RSS
{
    class Article;
    class Feed;
    class Folder;
    class Item;

    namespace Private
    {
        class FeedImpl;
        class Parser;
        struct ParsingResult;

        class Session : public QObject
        {
            Q_OBJECT
            Q_DISABLE_COPY(Session)

        public:
            Session(int refreshInterval, int maxArticlesPerFeed, QObject *parent = nullptr);
            ~Session() override;

            int maxArticlesPerFeed() const;
            void setMaxArticlesPerFeed(int n);

            int refreshInterval() const;
            void setRefreshInterval(int refreshInterval);

            Folder *addFolder(const QString &name, Folder &destFolder);
            Feed *addFeed(const QString &url, const QString &name, Folder &destFolder);
            void renameItem(Item &item, const QString &name);
            void moveItem(Item &item, Folder &destFolder, const QString &name = {});
            void removeItem(Item &item);

            bool addFolder(const QString &path, QString *error = nullptr);
            bool addFeed(const QString &url, const QString &path, QString *error = nullptr);
            bool moveItem(const QString &itemPath, const QString &destPath
                          , QString *error = nullptr);
            bool removeItem(const QString &itemPath, QString *error = nullptr);

            void refreshItem(qint64 itemId);

            QList<Item *> items() const;
            Item *itemByID(qint64 id) const;
            Item *itemByPath(const QString &path) const;
            QVector<Feed *> feeds() const;
            Feed *feedByURL(const QString &url) const;

            Folder *rootFolder() const;

        public slots:
            void refreshAll();

        signals:
            void itemAdded(Item *item);
            void itemPathChanged(Item *item);
            void itemAboutToBeRemoved(Item *item);
            void feedStateChanged(Feed *feed);

        private:
            void initializeDatabase();
            // migrate from old storage
            void migrate();
            void migrateFolder(const QJsonObject &jsonObj, qint64 folderId);
            void migrateFromLegacyData();
            void migrateFeedArticles(qint64 feedId, const QString &url, const QUuid &uid);
            void migrateFeedArticlesLegacy(qint64 feedId, const QString &url);

            void load();
            void loadFolder(qint64 folderId, Folder *folder);
            Folder *prepareItemDest(const QString &path, QString *error);
            void addItem(Item *item, Folder *destFolder);
            void cleanupItemData(const Item &item);
            void refreshItem(Item &item);

            void loadFeedArticles(qint64 feedId, FeedImpl &feed);
            void storeFeed(FeedImpl &feed);

            void handleFeedDownloadFinished(const Net::DownloadResult &result);
            void handleFeedParsingFinished(const ParsingResult &result);

            QString generateFeedName(const QString &baseName, Folder &destFolder);
            int updateFeedArticles(FeedImpl &feed, const QList<QVariantHash> &loadedArticles);

            QThread *m_workingThread;
            Parser *m_parser;
            QTimer m_refreshTimer;
            int m_refreshInterval;
            int m_maxArticlesPerFeed;
            QHash<qint64, Item *> m_itemsById;
            QHash<QString, Item *> m_itemsByPath;
            QHash<QString, FeedImpl *> m_feedsByURL;
        };
    }
}

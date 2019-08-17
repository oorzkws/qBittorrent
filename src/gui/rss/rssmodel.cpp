/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "rssmodel.h"

#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QHash>
#include <QIcon>
#include <QMimeData>

#include "base/exceptions.h"
#include "base/net/downloadmanager.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_item.h"
#include "base/rss/rss_session.h"
#include "base/utils/fs.h"
#include "uithememanager.h"

const QString INTERNAL_MIME_TYPE {QStringLiteral("application/x-qbittorrent-rssmodelidlist")};

namespace
{
    RSS::Item *getAttachedItem(const QModelIndex &index)
    {
        return static_cast<RSS::Item *>(index.internalPointer());
    }
}

RSSModel::RSSModel(QObject *parent)
    : QAbstractItemModel {parent}
{
    populate();

    const auto *rssManager = RSS::Session::instance();
    connect(rssManager, &RSS::Session::itemAdded, this, &RSSModel::itemAdded);
    connect(rssManager, &RSS::Session::itemPathChanged, this, &RSSModel::itemPathChanged);
    connect(rssManager, &RSS::Session::itemAboutToBeRemoved, this, &RSSModel::itemAboutToBeRemoved);
    connect(rssManager, &RSS::Session::feedStateChanged, this, &RSSModel::feedStateChanged);
}

RSSModel::~RSSModel()
{
}

int RSSModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant RSSModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    auto *item = static_cast<RSS::Item *>(getAttachedItem(index));
    Q_ASSERT(item);

    if (role == ItemPtrRole)
        return QVariant::fromValue<RSS::Item *>(item);

    if ((index.column() == 0) && (role == Qt::DecorationRole)) {
        if (qobject_cast<RSS::Folder *>(item))
            return UIThemeManager::instance()->getIcon(QStringLiteral("inode-directory"));

        auto *feed = static_cast<RSS::Feed *>(item);
        if (feed->isLoading())
            return QIcon(QStringLiteral(":/icons/loading.png"));
        if (feed->hasError())
            return UIThemeManager::instance()->getIcon(QStringLiteral("unavailable"));

        QIcon feedIcon = m_feedIcons.value(item);
        return ((feedIcon.availableSizes().size() == 0)
                ? UIThemeManager::instance()->getIcon(QStringLiteral("application-rss+xml"))
                : feedIcon);
    }

    if ((index.column() == 0) && (role == Qt::DisplayRole)) {
        if (item == RSS::Session::instance()->rootFolder())
            return QString(QStringLiteral("%1 (%2)")).arg(tr("Unread")).arg(item->unreadCount());
        return QString(QStringLiteral("%1 (%2/%3)"))
                .arg(item->name()).arg(item->unreadCount()).arg(item->articles().count());
    }

    return QVariant();
}

Qt::ItemFlags RSSModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return (QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled);

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (isSpecialItem(index)) {
        flags |= Qt::ItemNeverHasChildren;
    }
    else {
        flags |= Qt::ItemIsDragEnabled;

        if (qobject_cast<RSS::Folder *>(getAttachedItem(index)))
            flags |= Qt::ItemIsDropEnabled;
    }

    return flags;
}

QVariant RSSModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        if (section == 0)
            return tr("RSS feeds");

    return QVariant();
}

QModelIndex RSSModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0)
        return QModelIndex();

    const bool isTopLevel = !parent.isValid();
    if (isTopLevel && (row == 0)) {
        // Extra "Unread" item
        return createIndex(row, column, RSS::Session::instance()->rootFolder());
    }

    RSS::Folder *folder = isTopLevel
                   ? RSS::Session::instance()->rootFolder()
                   : qobject_cast<RSS::Folder *>(getAttachedItem(parent));
    Q_ASSERT(folder);

    const int itemPos = isTopLevel ? row - 1 : row;
    if (itemPos < folder->items().count())
        return createIndex(row, column, folder->items().at(itemPos));

    return QModelIndex();
}

QModelIndex RSSModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    auto *item = static_cast<RSS::Item *>(getAttachedItem(index));
    Q_ASSERT(item);

    auto parentFolder = qobject_cast<RSS::Folder *>(
                RSS::Session::instance()->itemByPath(RSS::Item::parentPath(item->path())));
    return this->index(parentFolder);
}

int RSSModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (isSpecialItem(parent))
        return 0;

    if (!parent.isValid())
        return RSS::Session::instance()->rootFolder()->items().count() + 1;

    auto *item = static_cast<RSS::Item *>(getAttachedItem(parent));
    Q_ASSERT(item);

    const auto *folder = qobject_cast<RSS::Folder *>(item);
    if (folder)
        return folder->items().count();

    return 0;
}

Qt::DropActions RSSModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList RSSModel::mimeTypes() const
{
    return QStringList {INTERNAL_MIME_TYPE};
}

QMimeData *RSSModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.count() <= 0)
        return nullptr;

    const QStringList types = mimeTypes();
    if (types.isEmpty())
        return nullptr;

    const QString format = types.at(0);
    Q_ASSERT(format == INTERNAL_MIME_TYPE);

    QByteArray encoded;
    QDataStream stream {&encoded, QIODevice::WriteOnly};
    for (const auto &index : indexes)
        stream << reinterpret_cast<quintptr>(getAttachedItem(index));

    QMimeData *data = new QMimeData;
    data->setData(format, encoded);
    return data;
}

bool RSSModel::dropMimeData(const QMimeData *data, Qt::DropAction action
                            , int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    if (action == Qt::IgnoreAction)
        return true;

    // check if the format is supported
    const QStringList types = mimeTypes();
    if (types.isEmpty())
        return false;

    const QString format = types.at(0);
    Q_ASSERT(format == INTERNAL_MIME_TYPE);

    if (!data->hasFormat(format))
        return false;

    RSS::Folder *destFolder = parent.isValid()
                              ? static_cast<RSS::Folder *>(getAttachedItem(parent))
                              : RSS::Session::instance()->rootFolder();

    // move as much items as possible
    QByteArray encoded = data->data(format);
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        quintptr itemPtr;
        stream >> itemPtr;
        RSS::Item *item = reinterpret_cast<RSS::Item *>(itemPtr);
        Q_ASSERT(item);

        try {
            RSS::Session::instance()->moveItem(item, destFolder);
        }
        catch (const RuntimeError &) {}
    }

    return true;
}

QModelIndex RSSModel::index(RSS::Item *item) const
{
    if (!item || (item == RSS::Session::instance()->rootFolder()))
        return {};

    auto parentFolder = qobject_cast<RSS::Folder *>(
                RSS::Session::instance()->itemByPath(RSS::Item::parentPath(item->path())));
    if (!parentFolder) return QModelIndex {};

    const QModelIndex parent = index(parentFolder);
    const int row = parentFolder->items().indexOf(item) + (parent.isValid() ? 0 : 1);
    return index(row, 0, parent);
}

bool RSSModel::isSpecialItem(const QModelIndex &index) const
{
    return (!index.parent().isValid() && (index.row() == 0));
}

void RSSModel::populate()
{
    // "Unread" item
    connect(RSS::Session::instance()->rootFolder(), &RSS::Item::unreadCountChanged, this, &RSSModel::itemUnreadCountChanged);

    populate(RSS::Session::instance()->rootFolder());
}

void RSSModel::populate(const RSS::Folder *rssFolder)
{
    const auto rssItems = rssFolder->items();
    for (auto *rssItem : rssItems) {
        addItem(rssItem);

        const auto *subfolder = qobject_cast<const RSS::Folder *>(rssItem);
        if (subfolder)
            populate(subfolder);
    }
}

void RSSModel::itemAdded(RSS::Item *rssItem)
{
    auto parentFolder = qobject_cast<RSS::Folder *>(
                RSS::Session::instance()->itemByPath(RSS::Item::parentPath(rssItem->path())));
    const auto parentIndex = index(parentFolder);
    const int row = index(rssItem).row();
    beginInsertRows(parentIndex, row, row);
    addItem(rssItem);
    endInsertRows();
}

void RSSModel::itemPathChanged(RSS::Item *rssItem)
{
    const auto itemIndex = index(rssItem);
    const auto parentIndex = itemIndex.parent();
    beginMoveRows(itemIndex.parent(), itemIndex.row(), itemIndex.row(), parentIndex, 0);
    endMoveRows();
}

void RSSModel::itemUnreadCountChanged(RSS::Item *rssItem)
{
    const auto i = index(rssItem);
    emit dataChanged(i, i, {Qt::DisplayRole});
}

void RSSModel::itemAboutToBeRemoved(RSS::Item *rssItem)
{
    const auto i = index(rssItem);
    if (!i.isValid()) return;

    beginRemoveRows(i.parent(), i.row(), i.row());
    endRemoveRows();
}

void RSSModel::feedStateChanged(RSS::Feed *rssFeed)
{
    QModelIndex i = index(rssFeed);
    while (i.isValid()) {
        emit dataChanged(i, i, {Qt::DecorationRole});
        i = i.parent();
    }
}

void RSSModel::feedIconDownloadFinished(RSS::Feed *feed, const QString &filePath)
{
    const auto feedIndex = index(feed);
    if (!feedIndex.isValid()) return;

    const QString iconPath = Utils::Fs::toUniformPath(filePath);
    const QIcon icon {iconPath};

    m_feedIcons[feed] = icon;
    if (!feed->isLoading() && !feed->hasError())
        emit dataChanged(feedIndex, feedIndex, {Qt::DecorationRole});

    connect(feed, &QObject::destroyed, this, [this, iconPath](QObject *feed)
    {
        Utils::Fs::forceRemove(iconPath);
        m_feedIcons.remove(feed);
    });
}

void RSSModel::addItem(RSS::Item *rssItem)
{
    if (auto *feed = qobject_cast<RSS::Feed *>(rssItem)) {
        // Download the RSS Feed icon
        // XXX: This works for most sites but it is not perfect
        const QUrl url = feed->url();
        auto iconUrl = QString("%1://%2/favicon.ico").arg(url.scheme(), url.host());
        Net::DownloadManager::instance()->download(
                    Net::DownloadRequest(iconUrl).saveToFile(true)
                    , feed, [this, feed](const Net::DownloadResult &result)
        {
            feedIconDownloadFinished(feed, result.filePath);
        });
    }

    connect(rssItem, &RSS::Item::unreadCountChanged, this, &RSSModel::itemUnreadCountChanged);
}

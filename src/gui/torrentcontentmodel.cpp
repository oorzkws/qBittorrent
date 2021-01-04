/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcontentmodel.h"

#include <algorithm>

#include <QDebug>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <Shellapi.h>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtWin>
#endif
#else
#include <QMimeDatabase>
#include <QMimeType>
#endif

#if defined Q_OS_WIN || defined Q_OS_MACOS
#define QBT_PIXMAP_CACHE_FOR_FILE_ICONS
#include <QPixmapCache>
#endif

#include "base/bittorrent/abstractfilestorage.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrentcontenthandler.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"
#include "raisedmessagebox.h"
#include "torrentcontentmodelfile.h"
#include "torrentcontentmodelfolder.h"
#include "torrentcontentmodelitem.h"
#include "uithememanager.h"

#ifdef Q_OS_MACOS
#include "macutilities.h"
#endif

namespace
{
    class UnifiedFileIconProvider : public QFileIconProvider
    {
    public:
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const override
        {
            Q_UNUSED(info);
            static QIcon cached = UIThemeManager::instance()->getIcon("text-plain");
            return cached;
        }
    };

#ifdef QBT_PIXMAP_CACHE_FOR_FILE_ICONS
    class CachingFileIconProvider : public UnifiedFileIconProvider
    {
    public:
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const final
        {
            const QString ext = info.suffix();
            if (!ext.isEmpty())
            {
                QPixmap cached;
                if (QPixmapCache::find(ext, &cached)) return {cached};

                const QPixmap pixmap = pixmapForExtension(ext);
                if (!pixmap.isNull())
                {
                    QPixmapCache::insert(ext, pixmap);
                    return {pixmap};
                }
            }
            return UnifiedFileIconProvider::icon(info);
        }

    protected:
        virtual QPixmap pixmapForExtension(const QString &ext) const = 0;
    };
#endif // QBT_PIXMAP_CACHE_FOR_FILE_ICONS

#if defined(Q_OS_WIN)
    // See QTBUG-25319 for explanation why this is required
    class WinShellFileIconProvider final : public CachingFileIconProvider
    {
        QPixmap pixmapForExtension(const QString &ext) const override
        {
            const QString extWithDot = QLatin1Char('.') + ext;
            SHFILEINFO sfi {};
            HRESULT hr = ::SHGetFileInfoW(extWithDot.toStdWString().c_str(),
                FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);
            if (FAILED(hr))
                return {};

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            auto iconPixmap = QPixmap::fromImage(QImage::fromHICON(sfi.hIcon));
#else
            QPixmap iconPixmap = QtWin::fromHICON(sfi.hIcon);
#endif
            ::DestroyIcon(sfi.hIcon);
            return iconPixmap;
        }
    };
#elif defined(Q_OS_MACOS)
    // There is a similar bug on macOS, to be reported to Qt
    // https://github.com/qbittorrent/qBittorrent/pull/6156#issuecomment-316302615
    class MacFileIconProvider final : public CachingFileIconProvider
    {
        QPixmap pixmapForExtension(const QString &ext) const override
        {
            return MacUtils::pixmapForExtension(ext, QSize(32, 32));
        }
    };
#else
    /**
     * @brief Tests whether QFileIconProvider actually works
     *
     * Some QPA plugins do not implement QPlatformTheme::fileIcon(), and
     * QFileIconProvider::icon() returns empty icons as the result. Here we ask it for
     * two icons for probably absent files and when both icons are null, we assume that
     * the current QPA plugin does not implement QPlatformTheme::fileIcon().
     */
    bool doesQFileIconProviderWork()
    {
        QFileIconProvider provider;
        const char PSEUDO_UNIQUE_FILE_NAME[] = "/tmp/qBittorrent-test-QFileIconProvider-845eb448-7ad5-4cdb-b764-b3f322a266a9";
        QIcon testIcon1 = provider.icon(QFileInfo(
            QLatin1String(PSEUDO_UNIQUE_FILE_NAME) + QLatin1String(".pdf")));
        QIcon testIcon2 = provider.icon(QFileInfo(
            QLatin1String(PSEUDO_UNIQUE_FILE_NAME) + QLatin1String(".png")));

        return (!testIcon1.isNull() || !testIcon2.isNull());
    }

    class MimeFileIconProvider : public UnifiedFileIconProvider
    {
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const override
        {
            const QMimeType mimeType = m_db.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
            QIcon res = QIcon::fromTheme(mimeType.iconName());
            if (!res.isNull())
            {
                return res;
            }

            res = QIcon::fromTheme(mimeType.genericIconName());
            if (!res.isNull())
            {
                return res;
            }

            return UnifiedFileIconProvider::icon(info);
        }

    private:
        QMimeDatabase m_db;
    };
#endif // Q_OS_WIN

    void applyToFiles(TorrentContentModelItem *item, const std::function<void (const int fileIndex)> &func)
    {
        if (item->itemType() == TorrentContentModelItem::FileType)
        {
            func(static_cast<TorrentContentModelFile *>(item)->fileIndex());
        }
        else
        {

            auto folder = static_cast<TorrentContentModelFolder *>(item);
            for (TorrentContentModelItem *childItem : folder->children())
                applyToFiles(childItem, func);
        }
    }

    template <typename T>
    T item_cast(TorrentContentModelItem *item)
    {
        using ItemType = typename std::remove_cv_t<typename std::remove_pointer_t<T>>;
        static_assert(std::is_base_of_v<TorrentContentModelItem, ItemType>
            , "item_cast<> can only be used with TorrentContentModelItem types.");

        if (item && (item->itemType() == ItemType::ITEM_TYPE))
            return static_cast<T>(item);

        return nullptr;
    }

    template <typename T>
    T item_cast(const TorrentContentModelItem *item)
    {
        using ItemType = typename std::remove_cv_t<typename std::remove_pointer_t<T>>;
        static_assert(std::is_base_of_v<TorrentContentModelItem, ItemType>
            , "item_cast<> can only be used with TorrentContentModelItem types.");

        if (item && (item->itemType() == ItemType::ITEM_TYPE))
            return static_cast<T>(item);

        return nullptr;
    }
}

TorrentContentModel::TorrentContentModel(QObject *parent)
    : QAbstractItemModel {parent}
    , m_headers {{tr("Name"), tr("Size"), tr("Progress")
                 , tr("Download Priority"), tr("Remaining"), tr("Availability")}}
    , m_rootItem {new TorrentContentModelFolder({})}
{
#if defined(Q_OS_WIN)
    m_fileIconProvider = new WinShellFileIconProvider;
#elif defined(Q_OS_MACOS)
    m_fileIconProvider = new MacFileIconProvider;
#else
    static bool doesBuiltInProviderWork = doesQFileIconProviderWork();
    m_fileIconProvider = doesBuiltInProviderWork ? new QFileIconProvider : new MimeFileIconProvider;
#endif
}

TorrentContentModel::~TorrentContentModel()
{
    delete m_fileIconProvider;
    delete m_rootItem;
}

void TorrentContentModel::clear()
{
    setHandler(nullptr);
}

void TorrentContentModel::setHandler(BitTorrent::TorrentContentHandler *torrentContentHandler)
{
    qDebug() << Q_FUNC_INFO;

    beginResetModel();

    if (m_torrentContentHandler)
    {
        disconnect(m_torrentContentHandler);
        m_torrentContentHandler = nullptr;
        m_filesIndex.clear();
        m_rootItem->deleteAllChildren();
    }

    m_torrentContentHandler = torrentContentHandler;
    if (!m_torrentContentHandler)
    {
        endResetModel();
        return;
    }

    connect(m_torrentContentHandler, &BitTorrent::TorrentContentHandler::metadataReceived
            , this, &TorrentContentModel::onMetadataReceived);
    connect(m_torrentContentHandler, &BitTorrent::TorrentContentHandler::filePriorityChanged
            , this, &TorrentContentModel::onFilePriorityChanged);
    connect(m_torrentContentHandler, &BitTorrent::TorrentContentHandler::fileRenamed
            , this, &TorrentContentModel::onFileRenamed);
    connect(m_torrentContentHandler, &BitTorrent::TorrentContentHandler::stateUpdated
            , this, &TorrentContentModel::onStateUpdated);

    if (m_torrentContentHandler->hasMetadata())
        populate();

    endResetModel();
}

BitTorrent::TorrentContentHandler *TorrentContentModel::handler() const
{
    return m_torrentContentHandler;
}

int TorrentContentModel::columnCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? getItem(parent) : m_rootItem)->columnCount();
}

int TorrentContentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    TorrentContentModelFolder *parentItem = !parent.isValid()
            ? m_rootItem
            : item_cast<TorrentContentModelFolder *>(getItem(parent));

    return parentItem ? parentItem->childCount() : 0;
}

bool TorrentContentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    TorrentContentModelItem *item = getItem(index);

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole))
    {
        if (value.toInt() == Qt::PartiallyChecked)
            return false;

        BitTorrent::DownloadPriority priority = ((value.toInt() == Qt::Unchecked)
                ? BitTorrent::DownloadPriority::Ignored
                : BitTorrent::DownloadPriority::Normal);

        applyToFiles(item, [priority, this](const int fileIndex)
        {
            m_torrentContentHandler->setFilePriority(fileIndex, priority);
        });

        return true;
    }

    if (role != Qt::EditRole)
        return false;

    switch (index.column())
    {
    case TorrentContentModelItem::COL_NAME:
        if (renameItem(item, value.toString()))
        {
            emit dataChanged(index, index, {role});
            return true;
        }
        break;

    case TorrentContentModelItem::COL_PRIO:
        applyToFiles(item, [priority = static_cast<BitTorrent::DownloadPriority>(value.toInt()), this](const int fileIndex)
        {
            m_torrentContentHandler->setFilePriority(fileIndex, priority);
        });
        return true;

    default:
        return false;
    }

    return false;
}

TorrentContentModelItem::ItemType TorrentContentModel::getItemType(const QModelIndex &index) const
{
    Q_ASSERT(index.isValid());
    return getItem(index)->itemType();
}

int TorrentContentModel::getFileIndex(const QModelIndex &index) const
{
    auto fileItem = item_cast<TorrentContentModelFile *>(getItem(index));
    return fileItem ? fileItem->fileIndex() : -1;
}

Path TorrentContentModel::getPath(const QModelIndex &index) const
{
    auto item = getItem(index);
    return item ? item->path() : Path();
}

QVariant TorrentContentModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    TorrentContentModelItem *item = getItem(index);

    switch (role)
    {
    case Qt::DecorationRole:
        if (index.column() != TorrentContentModelItem::COL_NAME)
            return {};

        if (item->itemType() == TorrentContentModelItem::FolderType)
            return m_fileIconProvider->icon(QFileIconProvider::Folder);

        return m_fileIconProvider->icon(QFileInfo(item->name()));

    case Qt::CheckStateRole:
        if (index.column() != TorrentContentModelItem::COL_NAME)
            return {};

        if (item->priority() == BitTorrent::DownloadPriority::Ignored)
            return Qt::Unchecked;

        if (item->priority() == BitTorrent::DownloadPriority::Mixed)
            return Qt::PartiallyChecked;

        return Qt::Checked;

    case Qt::TextAlignmentRole:
        if ((index.column() == TorrentContentModelItem::COL_SIZE)
            || (index.column() == TorrentContentModelItem::COL_REMAINING))
        {
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        }

        return {};

    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return item->displayData(index.column());

    case Roles::UnderlyingDataRole:
        return item->underlyingData(index.column());

    default:
        return {};
    }
}

Qt::ItemFlags TorrentContentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags {Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable};
    if (getItemType(index) == TorrentContentModelItem::FolderType)
        flags |= Qt::ItemIsAutoTristate;
    if (index.column() == TorrentContentModelItem::COL_PRIO)
        flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant TorrentContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    switch (role)
    {
    case Qt::DisplayRole:
        return m_headers.at(section);

    case Qt::TextAlignmentRole:
        if ((section == TorrentContentModelItem::COL_SIZE)
            || (section == TorrentContentModelItem::COL_REMAINING))
        {
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        }

        return {};

    default:
        return {};
    }
}

QModelIndex TorrentContentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && (parent.column() != 0))
        return {};

    if (column >= TorrentContentModelItem::NB_COL)
        return {};

    TorrentContentModelFolder *parentItem = (parent.isValid()
            ? static_cast<TorrentContentModelFolder *>(parent.internalPointer())
            : m_rootItem);
    Q_ASSERT(parentItem);

    if (row >= parentItem->childCount())
        return {};

    TorrentContentModelItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return {};
}

QModelIndex TorrentContentModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    TorrentContentModelItem *item = getItem(index);
    Q_ASSERT(item);

    TorrentContentModelItem *parentItem = item->parent();
    if (parentItem == m_rootItem)
        return {};

    return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex TorrentContentModel::getIndex(const TorrentContentModelItem *item) const
{
    if (item == m_rootItem)
        return {};

    return index(item->row(), TorrentContentModelItem::COL_NAME, getIndex(item->parent()));
}

void TorrentContentModel::onMetadataReceived()
{
    beginResetModel();
    populate();
    endResetModel();
}

void TorrentContentModel::onFilePriorityChanged(const int fileIndex, const BitTorrent::DownloadPriority priority)
{
    m_changedFilePriorities[fileIndex] = priority;
    if (m_deferredHandleFilePrioritiesChangedScheduled)
        return;

    m_deferredHandleFilePrioritiesChangedScheduled = true;

    QMetaObject::invokeMethod(this, &TorrentContentModel::handleFilePrioritiesChanged, Qt::QueuedConnection);
}

void TorrentContentModel::onFileRenamed(int fileIndex, const Path &filePath)
{
    m_renamedFiles[fileIndex] = filePath;
    if (m_deferredHandleFilesRenamedScheduled)
        return;

    m_deferredHandleFilesRenamedScheduled = true;

    QMetaObject::invokeMethod(this, &TorrentContentModel::handleFilesRenamed, Qt::QueuedConnection);
}

void TorrentContentModel::onStateUpdated()
{
    if (!m_torrentContentHandler->hasMetadata())
        return;

    const QVector<qreal> fp = m_torrentContentHandler->filesProgress();
    Q_ASSERT(m_filesIndex.size() == fp.size());

    const QVector<qreal> fa = m_torrentContentHandler->availableFileFractions();
    Q_ASSERT(m_filesIndex.size() == fa.size());

    for (int i = 0; i < m_filesIndex.size(); ++i)
    {
        TorrentContentModelFile *fileItem = m_filesIndex[i];
        fileItem->setProgress(fp[i]);
        fileItem->setAvailability(fa[i]);
    }

    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();

    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
}

void TorrentContentModel::handleFilePrioritiesChanged()
{
    for (auto it = m_changedFilePriorities.cbegin(); it != m_changedFilePriorities.cend(); ++it)
    {
        TorrentContentModelFile *fileItem = m_filesIndex.value(it.key());
        Q_ASSERT(fileItem);

        fileItem->setPriority(it.value());
    }

    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();

    m_changedFilePriorities.clear();
    m_deferredHandleFilePrioritiesChangedScheduled = false;

    emit dataChanged(this->index(0, 0), this->index((rowCount() - 1), (columnCount() - 1)));
    emit filePrioritiesChanged();
}

void TorrentContentModel::handleFilesRenamed()
{
    for (auto it = m_renamedFiles.cbegin(); it != m_renamedFiles.cend(); ++it)
    {
        TorrentContentModelFile *fileItem = m_filesIndex.value(it.key());
        Q_ASSERT(fileItem);

        const Path oldPath = fileItem->path();
        const Path &newPath = it.value();
        if (oldPath.toString() == newPath.toString())
            continue;

        fileItem->setName(newPath.filename());

        const Path oldParentPath = oldPath.parentPath();
        const Path newParentPath = newPath.parentPath();
        if (oldParentPath.toString() == newParentPath.toString())
        {
            const QModelIndex idx = getIndex(fileItem);
            emit dataChanged(idx, idx);
            continue;
        }

        TorrentContentModelFolder *oldParent = fileItem->parent();
        const QModelIndex itemIndex = getIndex(fileItem);
        const QModelIndex oldParentIndex = itemIndex.parent();
        beginRemoveRows(oldParentIndex, itemIndex.row(), itemIndex.row());
        oldParent->removeChild(fileItem);
        endRemoveRows();

        TorrentContentModelFolder *newParent = createFolderItem(newParentPath);
        const QModelIndex newParentIndex = getIndex(newParent);
        const int newRow = newParent->childCount();
        beginInsertRows(newParentIndex, newRow, newRow);
        newParent->appendChild(fileItem);
        endInsertRows();

        QModelIndex index = oldParentIndex;
        TorrentContentModelFolder *folderItem = oldParent;
        while (index.isValid() && (folderItem->childCount() == 0))
        {
            const QModelIndex parentIndex = index.parent();
            beginRemoveRows(parentIndex, index.row(), index.row());
            delete folderItem;
            endRemoveRows();

            index = parentIndex;
            folderItem = item_cast<TorrentContentModelFolder *>(getItem(index));
        }
    }

    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();

    m_renamedFiles.clear();
    m_deferredHandleFilesRenamedScheduled = false;
}

void TorrentContentModel::populate()
{
    const int filesCount = m_torrentContentHandler->filesCount();
    qDebug("Torrent contains %d files", filesCount);

    m_filesIndex.reserve(filesCount);
    for (int i = 0; i < filesCount; ++i)
    {
        TorrentContentModelFolder *parentFolder = createFolderItem(m_torrentContentHandler->filePath(i).parentPath());

        auto fileItem = new TorrentContentModelFile(m_torrentContentHandler->filePath(i).filename()
                , m_torrentContentHandler->fileSize(i), i);
        parentFolder->appendChild(fileItem);
        fileItem->setPriority(m_torrentContentHandler->filePriority(i));
        m_filesIndex.append(fileItem);
    }
}

TorrentContentModelFolder *TorrentContentModel::createFolderItem(const Path &path)
{
    if (path.isEmpty())
        return m_rootItem;

    const QList<QStringView> pathItems = QStringView(path.data()).split(u'/', Qt::SkipEmptyParts);
    TorrentContentModelItem *item = m_rootItem;
    // Iterate of parts of the path to create parent folders
    auto folder = static_cast<TorrentContentModelFolder *>(item);
    for (const QStringView &pathItem : pathItems)
    {
        const QString itemName = pathItem.toString();
        item = folder->itemByName(itemName);
        if (!item)
        {
            const int newRow = folder->childCount();
            beginInsertRows(getIndex(folder), newRow, newRow);
            item = new TorrentContentModelFolder(itemName);
            folder->appendChild(item);
            endInsertRows();
        }

        folder = item_cast<TorrentContentModelFolder *>(item);
        Q_ASSERT(folder);
    }

    return folder;
}

TorrentContentModelItem *TorrentContentModel::getItem(const Path &path) const
{
    const QList<QStringView> pathItems = QStringView(path.data()).split(u'/', Qt::SkipEmptyParts);
    TorrentContentModelItem *item = m_rootItem;
    // Iterate of parts of the path
    for (const QStringView &pathItem : pathItems)
    {
        auto folder = item_cast<TorrentContentModelFolder *>(item);
        if (!folder)
            return nullptr;

        const QString itemName = pathItem.toString();
        item = folder->itemByName(itemName);
    }

    return item;
}

TorrentContentModelItem *TorrentContentModel::getItem(const QModelIndex &index) const
{
    return static_cast<TorrentContentModelItem *>(index.internalPointer());
}

bool TorrentContentModel::renameItem(TorrentContentModelItem *item, const QString &newName)
{
    const QString oldName = item->name();
    if (newName == oldName)
        return false;  // Name did not change

    if (!Path(newName).parentPath().isEmpty())
    {
        RaisedMessageBox::warning(nullptr, tr("Rename error"), tr("Path separators aren't allowed in file/folder name."), QMessageBox::Ok);
        return false;
    }

    const Path oldPath = item->path();
    const Path newPath = oldPath.parentPath() / Path(newName);

    try
    {
        if (item->itemType() == TorrentContentModelItem::FileType)
            m_torrentContentHandler->renameFile(static_cast<TorrentContentModelFile *>(item)->fileIndex(), newPath);
        else
            m_torrentContentHandler->renameFolder(oldPath, newPath);

        item->setName(newName);
    }
    catch (const RuntimeError &error)
    {
        RaisedMessageBox::warning(nullptr, tr("Rename error"), error.message(), QMessageBox::Ok);
        return false;
    }

    return true;
}

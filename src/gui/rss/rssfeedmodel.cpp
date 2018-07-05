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

#include "rssfeedmodel.h"

#include <QDebug>
#include <QIcon>
#include <QPalette>

#include "base/rss/rss_article.h"
#include "base/rss/rss_item.h"

namespace
{
    RSS::Article *getAttachedArticle(const QModelIndex &index)
    {
        return static_cast<RSS::Article *>(index.internalPointer());
    }
}

RSSFeedModel::RSSFeedModel(RSS::Item *rssItem, QObject *parent)
    : QAbstractItemModel {parent}
{
    setRSSItem(rssItem);
}

RSSFeedModel::~RSSFeedModel()
{
}

void RSSFeedModel::setRSSItem(RSS::Item *rssItem)
{
    if (rssItem != m_rssItem) {
        beginResetModel();

        if (m_rssItem) {
            m_rssItem->disconnect(this);
            for (auto *rssArticle : m_rssItem->articles())
                rssArticle->disconnect(this);
        }

        m_rssItem = rssItem;
        if (m_rssItem) {
            connect(m_rssItem, &RSS::Item::newArticle, this, &RSSFeedModel::handleArticleAdded);
            connect(m_rssItem, &RSS::Item::articleAboutToBeRemoved, this, &RSSFeedModel::handleArticleAboutToBeRemoved);
            for (auto *rssArticle : m_rssItem->articles())
                addArticle(rssArticle);
        }

        endResetModel();
    }
}

int RSSFeedModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant RSSFeedModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.column() != 0))
        return {};

    RSS::Article *rssArticle = getAttachedArticle(index);
    Q_ASSERT(rssArticle);

    if (role == ItemPtrRole)
        return QVariant::fromValue<RSS::Article *>(rssArticle);

    if (role == Qt::DecorationRole) {
        if (rssArticle->isRead())
            return QIcon {":/icons/sphere.png"};
        return QIcon {":/icons/sphere2.png"};
    }

    if (role == Qt::ForegroundRole) {
        if (rssArticle->isRead())
            return QPalette().color(QPalette::Inactive, QPalette::WindowText);
        return QPalette().color(QPalette::Active, QPalette::Link);
    }

    if (role == Qt::DisplayRole)
        return rssArticle->title();

    return QVariant();
}

Qt::ItemFlags RSSFeedModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractItemModel::flags(index);

    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren);
}

QVariant RSSFeedModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        if (section == 0)
            return tr("RSS articles");

    return QVariant();
}

QModelIndex RSSFeedModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_rssItem || parent.isValid() || (column > 0))
        return {};

    const auto rssArticles = m_rssItem->articles();

    if (row < rssArticles.count())
        return createIndex(row, column, rssArticles.at(row));

    return {};
}

QModelIndex RSSFeedModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return {};
}

int RSSFeedModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_rssItem)
        return 0;

    return m_rssItem->articles().count();
}

QModelIndex RSSFeedModel::index(RSS::Article *rssArticle) const
{
    if (!m_rssItem || !rssArticle)
        return {};

    const int row = m_rssItem->articles().indexOf(rssArticle);
    return index(row, 0, {});
}

void RSSFeedModel::handleArticleAdded(RSS::Article *rssArticle)
{
    const int row = index(rssArticle).row();
    beginInsertRows({}, row, row);
    addArticle(rssArticle);
    endInsertRows();
}

void RSSFeedModel::handleArticleRead(RSS::Article *rssArticle)
{
    const auto i = index(rssArticle);
    emit dataChanged(i, i, {Qt::DisplayRole});
}

void RSSFeedModel::handleArticleAboutToBeRemoved(RSS::Article *rssArticle)
{
    const auto i = index(rssArticle);
    if (!i.isValid()) return;

    beginRemoveRows(i.parent(), i.row(), i.row());
    endRemoveRows();
}

void RSSFeedModel::addArticle(RSS::Article *rssArticle)
{
    connect(rssArticle, &RSS::Article::read, this, &RSSFeedModel::handleArticleRead);
}

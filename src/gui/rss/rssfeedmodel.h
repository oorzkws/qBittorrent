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

#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>

namespace RSS
{
    class Article;
    class Item;
}

class RSSFeedModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY(RSSFeedModel)

public:
    enum ItemDataRole
    {
        ItemPtrRole = Qt::UserRole
    };

    explicit RSSFeedModel(RSS::Item *rssItem, QObject *parent = nullptr);
    ~RSSFeedModel() override;

    void setRSSItem(RSS::Item *rssItem);

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

private:
    QModelIndex index(RSS::Article *rssArticle) const;

    void handleArticleAdded(RSS::Article *rssArticle);
    void handleArticleRead(RSS::Article *rssArticle);
    void handleArticleAboutToBeRemoved(RSS::Article *rssArticle);

    void addArticle(RSS::Article *rssArticle);

    RSS::Item *m_rssItem = nullptr;
};

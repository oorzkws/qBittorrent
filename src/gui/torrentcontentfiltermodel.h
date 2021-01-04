/*
 * Bittorrent Client using Qt and libtorrent.
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

#pragma once

#include <QSortFilterProxyModel>

#include "base/pathfwd.h"
#include "base/utils/compare.h"
#include "torrentcontentmodelitem.h"

class TorrentContentModel;

class TorrentContentFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_DISABLE_COPY(TorrentContentFilterModel)

public:
    explicit TorrentContentFilterModel(QObject *parent = nullptr);

    TorrentContentModel *sourceModel() const;
    TorrentContentModelItem::ItemType getItemType(const QModelIndex &index) const;
    int getFileIndex(const QModelIndex &index) const;
    Path getPath(const QModelIndex &index) const;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QModelIndex parent(const QModelIndex &child) const override;

public slots:
    void selectAll();
    void selectNone();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool hasFiltered(const QModelIndex &folder) const;

    Utils::Compare::NaturalLessThan<Qt::CaseInsensitive> m_naturalLessThan;
};

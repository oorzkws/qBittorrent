/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "resumedatastorage.h"

#include <utility>

#include <QHash>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

const int TORRENTIDLIST_TYPEID = qRegisterMetaType<QVector<BitTorrent::TorrentID>>();

BitTorrent::ResumeDataStorage::ResumeDataStorage(const Path &path, QObject *parent)
    : QObject(parent)
    , m_path {path}
{
}

Path BitTorrent::ResumeDataStorage::path() const
{
    return m_path;
}

void BitTorrent::ResumeDataStorage::loadAll() const
{
    m_loadedResumeData.reserve(1024);

    auto *loadingThread = QThread::create([this]()
    {
        doLoadAll();
    });
    connect(loadingThread, &QThread::finished, loadingThread, &QObject::deleteLater);
    loadingThread->start();
}

QVector<BitTorrent::LoadedResumeData> BitTorrent::ResumeDataStorage::fetchLoadedResumeData() const
{
    const QMutexLocker locker {&m_loadedResumeDataMutex};

    const QVector<BitTorrent::LoadedResumeData> loadedResumeData = m_loadedResumeData;
    m_loadedResumeData.clear();

    return loadedResumeData;
}

void BitTorrent::ResumeDataStorage::onResumeDataLoaded(const TorrentID &torrentID, const LoadResumeDataResult &loadResumeDataResult) const
{
    const QMutexLocker locker {&m_loadedResumeDataMutex};
    m_loadedResumeData.append({torrentID, loadResumeDataResult});
}

/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2019  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez
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

#include "qbittorrentimpl.h"
#include "preferences.h"

QBittorrentImpl::QBittorrentImpl(QCoreApplication &app)
    : QBittorrent {app}
{
}

QBittorrentImpl::~QBittorrentImpl()
{
}

bool QBittorrentImpl::createComponents()
{
//    Net::ProxyConfigurationManager::initInstance();
//    Net::DownloadManager::initInstance();
//#ifdef DISABLE_GUI
//    IconProvider::initInstance();
//#else
//    GuiIconProvider::initInstance();
//#endif

//    try {
//        BitTorrent::Session::initInstance();
//        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &QBittorrentImpl::torrentFinished);
//        connect(BitTorrent::Session::instance(), &BitTorrent::Session::allTorrentsFinished, this, &QBittorrentImpl::allTorrentsFinished, Qt::QueuedConnection);

//#ifndef DISABLE_COUNTRIES_RESOLUTION
//        Net::GeoIPManager::initInstance();
//#endif
//        ScanFoldersModel::initInstance(this);

//#ifndef DISABLE_WEBUI
//        m_webui = new WebUI;
//#ifdef DISABLE_GUI
//        if (m_webui->isErrored())
//            return false;
//        connect(m_webui, &WebUI::fatalError, this, []() { QCoreApplication::exit(1); });
//#endif // DISABLE_GUI
//#endif // DISABLE_WEBUI

//        new RSS::Session; // create RSS::Session singleton
//        new RSS::AutoDownloader; // create RSS::AutoDownloader singleton
//    }
//    catch (const RuntimeError &err) {
//#ifdef DISABLE_GUI
//        fprintf(stderr, "%s", err.what());
//#else
//        QMessageBox msgBox;
//        msgBox.setIcon(QMessageBox::Critical);
//        msgBox.setText(tr("Application failed to start."));
//        msgBox.setInformativeText(err.message());
//        msgBox.show(); // Need to be shown or to moveToCenter does not work
//        msgBox.move(Utils::Misc::screenCenter(&msgBox));
//        msgBox.exec();
//#endif
//        return false;
//    }

    return true;
}

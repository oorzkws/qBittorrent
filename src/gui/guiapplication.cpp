/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2019, 2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "guiapplication.h"

#include <QApplication>
#include <QMessageBox>
#include <QPixmapCache>

#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#endif // Q_OS_MACOS

#ifdef Q_OS_WIN
#include <QSessionManager>
#endif // Q_OS_WIN

#include "base/preferences.h"
#include "addnewtorrentdialog.h"
#include "mainwindow.h"
#include "shutdownconfirmdialog.h"
#include "uithememanager.h"
#include "utils.h"

const int PIXMAP_CACHE_SIZE = 64 * 1024 * 1024;  // 64MiB

GUIApplication::GUIApplication(int &argc, char **argv)
    : Application(new QApplication(argc, argv))
{
    auto qtApp = static_cast<QApplication *>(this->qtApp());
    qtApp->setDesktopFileName("org.qbittorrent.qBittorrent");
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    qtApp->setAttribute(Qt::AA_UseHighDpiPixmaps, true);  // opt-in to the high DPI pixmap support
#endif
    qtApp->setQuitOnLastWindowClosed(false);

#if defined(Q_OS_MACOS)
    // On OS X the standard is to not show icons in the menus
    qtApp->setAttribute(Qt::AA_DontShowIconsInMenus);
#else
    if (!Preferences::instance()->iconsInMenusEnabled())
        qtApp->setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    const QString localeStr = Preferences::instance()->getLocale();
    if (localeStr.startsWith("ar") || localeStr.startsWith("he"))
        qtApp->setLayoutDirection(Qt::RightToLeft);
    else
        qtApp->setLayoutDirection(Qt::LeftToRight);

    QPixmapCache::setCacheLimit(PIXMAP_CACHE_SIZE);

#ifdef Q_OS_WIN
    connect(qtApp, &QGuiApplication::commitDataRequest, this, &GUIApplication::shutdownCleanup, Qt::DirectConnection);
#endif
}

MainWindow *GUIApplication::mainWindow()
{
    return m_mainWindow;
}

GUIApplication::addTorrent(const QString &torrentSource, const BitTorrent::AddTorrentParams &torrentParams)
{
    // There are two circumstances in which we want to show the torrent
    // dialog. One is when the application settings specify that it should
    // be shown and skipTorrentDialog is undefined. The other is when
    // skipTorrentDialog is false, meaning that the application setting
    // should be overridden.
    const bool showDialogForThisTorrent = !skipTorrentDialog.value_or(!AddNewTorrentDialog::isEnabled());
    if (showDialogForThisTorrent)
        AddNewTorrentDialog::show(torrentSource, torrentParams, m_mainWindow);
    else
        Application::addTorrent(torrentSource, torrentParams);
}

bool GUIApplication::initializeComponents()
{
    if (!Application::initializeComponents())
        return false;

    UIThemeManager::initInstance();
    m_mainWindow = new MainWindow(this);

    return true;
}

bool GUIApplication::processParam(const QStringView param, BitTorrent::AddTorrentParams &torrentParams) const
{
    if (Application::processParam(param, torrentParams))
        return true;

    std::optional<bool> skipTorrentDialog;

    if (param.startsWith(u"@skipDialog="))
    {
        skipTorrentDialog = (param.mid(12).toInt() != 0);
        return true;
    }

    return false;
}

void GUIApplication::cleanup()
{
    // cleanup() can be called multiple times during shutdown. We only need it once.
    static QAtomicInt alreadyDone;
    if (!alreadyDone.testAndSetAcquire(0, 1))
        return;

    //#ifndef DISABLE_GUI
    //    if (m_mainWindow)
    //    {
    //        // Hide the window and don't leave it on screen as
    //        // unresponsive. Also for Windows take the WinId
    //        // after it's hidden, because hide() may cause a
    //        // WinId change.
    //        m_mainWindow->hide();

    //#ifdef Q_OS_WIN
    //        ::ShutdownBlockReasonCreate(reinterpret_cast<HWND>(m_mainWindow->effectiveWinId())
    //                                    , tr("Saving torrent progress...").toStdWString().c_str());
    //#endif // Q_OS_WIN

    //        // Do manual cleanup in MainWindow to force widgets
    //        // to save their Preferences, stop all timers and
    //        // delete as many widgets as possible to leave only
    //        // a 'shell' MainWindow.
    //        // We need a valid window handle for Windows Vista+
    //        // otherwise the system shutdown will continue even
    //        // though we created a ShutdownBlockReason
    //        m_mainWindow->cleanup();
    //    }
    //#endif // DISABLE_GUI

    //#ifndef DISABLE_WEBUI
    //    delete m_webui;
    //#endif

    //    delete RSS::AutoDownloader::instance();
    //    delete RSS::Session::instance();

    //    TorrentFilesWatcher::freeInstance();
    //    BitTorrent::Session::freeInstance();
    //    Net::GeoIPManager::freeInstance();
    //    Net::DownloadManager::freeInstance();
    //    Net::ProxyConfigurationManager::freeInstance();
    //    Preferences::freeInstance();
    //    SettingsStorage::freeInstance();
    //    delete m_fileLogger;
    //    Logger::freeInstance();
    //    IconProvider::freeInstance();
    //    SearchPluginManager::freeInstance();
    //    Utils::Fs::removeDirRecursively(Utils::Fs::tempPath());

    //#ifndef DISABLE_GUI
    //    if (m_mainWindow)
    //    {
    //#ifdef Q_OS_WIN
    //        ::ShutdownBlockReasonDestroy(reinterpret_cast<HWND>(m_mainWindow->effectiveWinId()));
    //#endif // Q_OS_WIN
    //        delete m_mainWindow;
    //        UIThemeManager::freeInstance();
    //    }
    //#endif // DISABLE_GUI

    //    Profile::freeInstance();

    //    if (m_shutdownAct != ShutdownDialogAction::Exit)
    //    {
    //        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
    //        Utils::Misc::shutdownComputer(m_shutdownAct);
    //    }
}

#ifdef Q_OS_WIN
void GUIApplication::shutdownCleanup(QSessionManager &manager)
{
    Q_UNUSED(manager);

    // This is only needed for a special case on Windows XP.
    // (but is called for every Windows version)
    // If a process takes too much time to exit during OS
    // shutdown, the OS presents a dialog to the user.
    // That dialog tells the user that qbt is blocking the
    // shutdown, it shows a progress bar and it offers
    // a "Terminate Now" button for the user. However,
    // after the progress bar has reached 100% another button
    // is offered to the user reading "Cancel". With this the
    // user can cancel the **OS** shutdown. If we don't do
    // the cleanup by handling the commitDataRequest() signal
    // and the user clicks "Cancel", it will result in qbt being
    // killed and the shutdown proceeding instead. Apparently
    // aboutToQuit() is emitted too late in the shutdown process.
    cleanup();

    // According to the qt docs we shouldn't call quit() inside a slot.
    // aboutToQuit() is never emitted if the user hits "Cancel" in
    // the above dialog.
    QTimer::singleShot(0, qtApp(), &QCoreApplication::quit);
}
#endif

void GUIApplication::activate()
{
    m_mainWindow->activate(); // show UI
}

bool GUIApplication::confirmAutoExit(const ShutdownDialogAction action) const
{
    return ShutdownConfirmDialog::askForConfirmation(m_mainWindow, action);
}

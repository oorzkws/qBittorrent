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

#include "qbittorrentguiimpl.h"

#include <QtGlobal>

#include <algorithm>

#ifdef Q_OS_WIN
#include <memory>
#include <Windows.h>
#include <Shellapi.h>
#endif

#include <QAtomicInt>
#include <QDebug>

#include <QMessageBox>
#include <QPushButton>
#ifdef Q_OS_WIN
#include <QSessionManager>
#endif // Q_OS_WIN
#ifdef Q_OS_MAC
#include <QFileOpenEvent>
#endif // Q_OS_MAC

#include "base/bittorrent/session.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "webui/webui.h"
#include "addnewtorrentdialog.h"
#include "guiiconprovider.h"
#include "mainwindow.h"
#include "shutdownconfirmdialog.h"

QBittorrentGUIImpl::QBittorrentGUIImpl(QApplication &app)
    : QBittorrent {app}
{
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);  // opt-in to the high DPI pixmap support
    app.setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    connect(&app, &QGuiApplication::commitDataRequest, this, &QBittorrentGUIImpl::shutdownCleanup, Qt::DirectConnection);
#endif

    const QString localeStr = Preferences::instance()->getLocale();
    if (localeStr.startsWith("ar") || localeStr.startsWith("he")) {
        qDebug("Right to Left mode");
        QApplication::setLayoutDirection(Qt::RightToLeft);
    }
    else {
        QApplication::setLayoutDirection(Qt::LeftToRight);
    }
}

QBittorrentGUIImpl::~QBittorrentGUIImpl()
{
}

MainWindow *QBittorrentGUIImpl::mainWindow()
{
    return m_window;
}

void QBittorrentGUIImpl::addTorrent(const QString &source, const BitTorrent::AddTorrentParams &torrentParams)
{
    // There are two circumstances in which we want to show the torrent
    // dialog. One is when the application settings specify that it should
    // be shown and skipTorrentDialog is undefined. The other is when
    // skipTorrentDialog is false, meaning that the application setting
    // should be overridden.
    const bool showDialogForThisTorrent =
            ((AddNewTorrentDialog::isEnabled() && (skipTorrentDialog == TriStateBool::Undefined))
             || (skipTorrentDialog == TriStateBool::False));
    if (showDialogForThisTorrent)
        AddNewTorrentDialog::show(source, torrentParams, m_window);
    else
        BitTorrent::Session::instance()->addTorrent(source, torrentParams);
}

bool QBittorrentGUIImpl::createComponents()
{
    if (!QBittorrent::createComponents())
        return false;

    m_window = new MainWindow;

    return true;
}

bool QBittorrentGUIImpl::confirmShutdown() const
{
    return ((ShutdownConfirmDialog {m_window, m_shutdownAction}).exec() == QDialog::Accepted);
}

void QBittorrentGUIImpl::showStartupInfo() const
{
    // does nothing when GUI is enabled
}

void QBittorrentGUIImpl::showErrorMessage(const QString &message) const
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(tr("Application failed to start."));
    msgBox.setInformativeText(message);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
}

void QBittorrentGUIImpl::activate() const
{
    m_window->activate();
}

bool QBittorrentGUIImpl::userAgreesWithLegalNotice()
{
    QMessageBox msgBox;
    msgBox.setText(tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(tr("Legal notice"));
    msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
    QAbstractButton *agreeButton = msgBox.addButton(tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
    if (msgBox.clickedButton() == agreeButton) {
        // Save the answer
        Preferences::instance()->setAcceptedLegal(true);
        return true;
    }

    return false;
}

#ifdef Q_OS_WIN
void QBittorrentGUIImpl::displayUsage() const
{
    QMessageBox msgBox {QMessageBox::Information, tr("Help")
                , m_commandLineParameters.makeUsage(), QMessageBox::Ok};
    msgBox.show(); // Need to be shown or moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
}

IconProvider *QBittorrentGUIImpl::createIconProvider()
{
    return new GuiIconProvider {this};
}

#ifndef DISABLE_WEBUI
WebUI *QBittorrentGUIImpl::createWebUI()
{
    return new WebUI;
}
#endif

void QBittorrentGUIImpl::beginCleanup()
{
    if (m_window) {
        // Hide the window and don't leave it on screen as
        // unresponsive. Also for Windows take the WinId
        // after it's hidden, because hide() may cause a
        // WinId change.
        m_window->hide();

#ifdef Q_OS_WIN
        using PSHUTDOWNBRCREATE = BOOL (WINAPI *)(HWND, LPCWSTR);
        const auto shutdownBRCreate = Utils::Misc::loadWinAPI<PSHUTDOWNBRCREATE>("User32.dll", "ShutdownBlockReasonCreate");
        // Only available on Vista+
        if (shutdownBRCreate)
            shutdownBRCreate(reinterpret_cast<HWND>(m_window->effectiveWinId()), tr("Saving torrent progress...").toStdWString().c_str());
#endif // Q_OS_WIN

        // Do manual cleanup in MainWindow to force widgets
        // to save their Preferences, stop all timers and
        // delete as many widgets as possible to leave only
        // a 'shell' MainWindow.
        // We need a valid window handle for Windows Vista+
        // otherwise the system shutdown will continue even
        // though we created a ShutdownBlockReason
        m_window->cleanup();
    }

    QBittorrent::beginCleanup();
}

void QBittorrentGUIImpl::endCleanup()
{
    QBittorrent::endCleanup();

    if (m_window) {
#ifdef Q_OS_WIN
        using PSHUTDOWNBRDESTROY = BOOL (WINAPI *)(HWND);
        const auto shutdownBRDestroy = Utils::Misc::loadWinAPI<PSHUTDOWNBRDESTROY>("User32.dll", "ShutdownBlockReasonDestroy");
        // Only available on Vista+
        if (shutdownBRDestroy)
            shutdownBRDestroy(reinterpret_cast<HWND>(m_window->effectiveWinId()));
#endif // Q_OS_WIN
        delete m_window;
    }
}

void QBittorrentGUIImpl::shutdownCleanup(QSessionManager &manager)
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
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}
#endif

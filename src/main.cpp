/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QtGlobal>

#include <csignal>
#include <cstdlib>
#include <memory>

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
#include <unistd.h>
#endif

#include <QDebug>
#include <QScopedPointer>
#include <QThread>

#ifndef DISABLE_GUI
// GUI-only includes
#include <QApplication>
#include <QFont>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSplashScreen>
#include <QTimer>

#ifdef QBT_STATIC_QT
#include <QtPlugin>
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // QBT_STATIC_QT

#include "gui/qbittorrentguiimpl.h"

#else
// NoGUI-only includes
#include <cstdio>
#include <QCoreApplication>
#include "base/qbittorrentimpl.h"
#endif // DISABLE_GUI

#ifdef STACKTRACE
#ifdef Q_OS_UNIX
#include "stacktrace.h"
#else
#include "stacktrace_win.h"
#include "stacktracedialog.h"
#endif // Q_OS_UNIX
#endif //STACKTRACE

// Signal handlers
void sigNormalHandler(int signum);
#ifdef STACKTRACE
void sigAbnormalHandler(int signum);
#endif
// sys_signame[] is only defined in BSD
const char *const sysSigName[] = {
#if defined(Q_OS_WIN)
    "", "", "SIGINT", "", "SIGILL", "", "SIGABRT_COMPAT", "", "SIGFPE", "",
    "", "SIGSEGV", "", "", "", "SIGTERM", "", "", "", "",
    "", "SIGBREAK", "SIGABRT", "", "", "", "", "", "", "",
    "", ""
#else
    "", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
    "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
    "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
    "SIGPWR", "SIGUNUSED"
#endif
};

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char *str);
#endif

void displayBadArgMessage(const QString &message);

#if !defined(DISABLE_GUI)
void showSplashScreen();
#endif  // DISABLE_GUI

// Main
int main(int argc, char *argv[])
{
#ifndef DISABLE_GUI
    QApplication app {argc, argv};
    std::unique_ptr<QBittorrent> qBittorrent = std::unique_ptr<QBittorrent>(new QBittorrentGUIImpl {app});
#else
    QCoreApplication app {argc, argv};
    std::unique_ptr<QBittorrent> qBittorrent = std::unique_ptr<QBittorrent>(new QBittorrentImpl {app});
#endif

    try {
        // Check if qBittorrent is already running for this user
        if (!instanceManager->isFirstInstance()) {
#ifdef DISABLE_GUI
            if (params.shouldDaemonize) {
                throw CommandLineParameterError {
                    app.translate("main", "You cannot use %1: qBittorrent is already running for this user.")
                            .arg(QLatin1String {"-d (or --daemon)"})};
            }
            else
#endif
            qDebug("qBittorrent is already running for this user.");

            QThread::msleep(300);
            instanceManager->sendMessage(params.paramList().join(PARAMS_SEPARATOR));

            return EXIT_SUCCESS;
        }

#if defined(Q_OS_WIN)
        // This affects only Windows apparently and Qt5.
        // When QNetworkAccessManager is instantiated it regularly starts polling
        // the network interfaces to see what's available and their status.
        // This polling creates jitter and high ping with wifi interfaces.
        // So here we disable it for lack of better measure.
        // It will also spew this message in the console: QObject::startTimer: Timers cannot have negative intervals
        // For more info see:
        // 1. https://github.com/qbittorrent/qBittorrent/issues/4209
        // 2. https://bugreports.qt.io/browse/QTBUG-40332
        // 3. https://bugreports.qt.io/browse/QTBUG-46015

        qputenv("QT_BEARER_POLL_TIMEOUT", QByteArray::number(-1));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        // this is the default in Qt6
        app.setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#endif // Q_OS_WIN

#if defined(Q_OS_MAC)
        // Since Apple made difficult for users to set PATH, we set here for convenience.
        // Users are supposed to install Homebrew Python for search function.
        // For more info see issue #5571.
        QByteArray path = "/usr/local/bin:";
        path += qgetenv("PATH");
        qputenv("PATH", path.constData());

        // On OS X the standard is to not show icons in the menus
        app.setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

#ifndef DISABLE_GUI
        if (!upgrade()) return EXIT_FAILURE;
#else
        if (!upgrade(!params.shouldDaemonize
                     && isatty(fileno(stdin))
                     && isatty(fileno(stdout)))) return EXIT_FAILURE;
#endif
#ifdef DISABLE_GUI
        if (params.shouldDaemonize) {
            instanceManager.reset(); // Destroy current application instance manager
            if (daemon(1, 0) == 0) {
                instanceManager.reset(new ApplicationInstanceManager {appId, &app});
                if (!instanceManager->isFirstInstance()) {
                    // Another instance had time to start.
                    return EXIT_FAILURE;
                }
            }
            else {
                qCritical("Something went wrong while daemonizing, exiting...");
                return EXIT_FAILURE;
            }
        }
#else
        if (!(params.noSplash || Preferences::instance()->isSplashScreenDisabled()))
            showSplashScreen();
#endif

        signal(SIGINT, sigNormalHandler);
        signal(SIGTERM, sigNormalHandler);
#ifdef STACKTRACE
        signal(SIGABRT, sigAbnormalHandler);
        signal(SIGSEGV, sigAbnormalHandler);
#endif

//        qBittorrent->run(params.paramList());
        return app.exec();
    }
    catch (CommandLineParameterError &er) {
        displayBadArgMessage(er.messageForUser());
        return EXIT_FAILURE;
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
void reportToUser(const char *str)
{
    const size_t strLen = strlen(str);
    if (write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen)) {
        const auto dummy = write(STDOUT_FILENO, str, strLen);
        Q_UNUSED(dummy);
    }
}
#endif

void sigNormalHandler(int signum)
{
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char msg1[] = "Catching signal: ";
    const char msg2[] = "\nExiting cleanly\n";
    reportToUser(msg1);
    reportToUser(sysSigName[signum]);
    reportToUser(msg2);
#endif // !defined Q_OS_WIN && !defined Q_OS_HAIKU
    signal(signum, SIG_DFL);
    qApp->exit();  // unsafe, but exit anyway
}

#ifdef STACKTRACE
void sigAbnormalHandler(int signum)
{
    const char *sigName = sysSigName[signum];
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
    const char msg[] = "\n\n*************************************************************\n"
        "Please file a bug report at http://bug.qbittorrent.org and provide the following information:\n\n"
        "qBittorrent version: " QBT_VERSION "\n\n"
        "Caught signal: ";
    reportToUser(msg);
    reportToUser(sigName);
    reportToUser("\n");
    print_stacktrace();  // unsafe
#endif

#if defined Q_OS_WIN
    StacktraceDialog dlg;  // unsafe
    dlg.setStacktraceString(QLatin1String(sigName), straceWin::getBacktrace());
    dlg.exec();
#endif

    signal(signum, SIG_DFL);
    raise(signum);
}
#endif // STACKTRACE

#if !defined(DISABLE_GUI)
void showSplashScreen()
{
    QPixmap splashImg(":/icons/skin/splash.png");
    QPainter painter(&splashImg);
    const QString version = QBT_VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
    QSplashScreen *splash = new QSplashScreen(splashImg);
    splash->show();
    QTimer::singleShot(1500, splash, &QObject::deleteLater);
    qApp->processEvents();
}
#endif  // DISABLE_GUI

void displayBadArgMessage(const QString &message)
{
    const QString help = QObject::tr("Run application with -h option to read about command line parameters.");
#ifdef Q_OS_WIN
    QMessageBox msgBox(QMessageBox::Critical, QObject::tr("Bad command line"),
                       message + QLatin1Char('\n') + help, QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
#else
    const QString errMsg = QObject::tr("Bad command line: ") + '\n'
        + message + '\n'
        + help + '\n';
    fprintf(stderr, "%s", qUtf8Printable(errMsg));
#endif
}

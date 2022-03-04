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

#include "application.h"

#include <cstdio>

#ifdef Q_OS_WIN
#include <memory>
#include <Windows.h>
#include <Shellapi.h>
#endif

#include <QAtomicInt>
#include <QDebug>
#include <QLibraryInfo>
#include <QProcess>

#include "applicationinstancemanager.h"
#include "bittorrent/infohash.h"
#include "bittorrent/session.h"
#include "bittorrent/torrent.h"
#include "exceptions.h"
#include "filelogger.h"
#include "global.h"
#include "iconprovider.h"
#include "logger.h"
#include "net/downloadmanager.h"
#include "net/geoipmanager.h"
#include "net/proxyconfigurationmanager.h"
#include "net/smtp.h"
#include "path.h"
#include "preferences.h"
#include "profile.h"
#include "rss/rss_autodownloader.h"
#include "rss/rss_session.h"
#include "search/searchpluginmanager.h"
#include "settingsstorage.h"
#include "torrentfileswatcher.h"
#include "utils/fs.h"
#include "utils/misc.h"
#include "version.h"
#ifndef DISABLE_WEBUI
#include "webui/webui.h"
#endif

namespace
{
#define SETTINGS_KEY(name) "Application/" name
#define FILELOGGER_SETTINGS_KEY(name) (SETTINGS_KEY("FileLogger/") name)

    const QChar PARAMS_SEPARATOR = u'|';

    const Path DEFAULT_PORTABLE_MODE_PROFILE_DIR {u"profile"_qs};

    const int MIN_FILELOG_SIZE = 1024; // 1KiB
    const int MAX_FILELOG_SIZE = 1000 * 1024 * 1024; // 1000MiB
    const int DEFAULT_FILELOG_SIZE = 65 * 1024; // 65KiB
}

Application::Application(int &argc, char **argv)
    : Application(new QCoreApplication(argc, argv))
{
}

Application::Application(QCoreApplication *qtApp)
    : QObject()
    , m_qtApp(qtApp)
    , m_commandLineArgs(parseCommandLine(qtApp->arguments()))
#ifdef Q_OS_WIN
    , m_storeMemoryWorkingSetLimit(SETTINGS_KEY("MemoryWorkingSetLimit"))
#endif
    , m_storeFileLoggerEnabled(FILELOGGER_SETTINGS_KEY("Enabled"))
    , m_storeFileLoggerBackup(FILELOGGER_SETTINGS_KEY("Backup"))
    , m_storeFileLoggerDeleteOld(FILELOGGER_SETTINGS_KEY("DeleteOld"))
    , m_storeFileLoggerMaxSize(FILELOGGER_SETTINGS_KEY("MaxSizeBytes"))
    , m_storeFileLoggerAge(FILELOGGER_SETTINGS_KEY("Age"))
    , m_storeFileLoggerAgeType(FILELOGGER_SETTINGS_KEY("AgeType"))
    , m_storeFileLoggerPath(FILELOGGER_SETTINGS_KEY("Path"))
{
    Q_ASSERT(m_qtApp);

    qRegisterMetaType<Log::Msg>("Log::Msg");
    qRegisterMetaType<Log::Peer>("Log::Peer");

    m_qtApp->setApplicationName("qBittorrent");
    m_qtApp->setOrganizationDomain("qbittorrent.org");

    m_qtApp->installEventFilter(this);

    connect(m_qtApp, &QCoreApplication::aboutToQuit, this, &Application::cleanup);

    const auto portableProfilePath = Path(QCoreApplication::applicationDirPath()) / DEFAULT_PORTABLE_MODE_PROFILE_DIR;
    const bool portableModeEnabled = m_commandLineArgs.profileDir.isEmpty() && portableProfilePath.exists();

    const Path profileDir = portableModeEnabled
        ? portableProfilePath
        : m_commandLineArgs.profileDir;
    Profile::initInstance(profileDir, m_commandLineArgs.configurationName,
                        (m_commandLineArgs.relativeFastresumePaths || portableModeEnabled));

    m_instanceManager = new ApplicationInstanceManager(Profile::instance()->location(SpecialFolder::Config), this);

    Logger::initInstance();
    SettingsStorage::initInstance();
    Preferences::initInstance();

    initializeTranslation();

    if (m_commandLineArgs.webUiPort > 0) // it will be -1 when user did not set any value
        Preferences::instance()->setWebUiPort(m_commandLineArgs.webUiPort);

    connect(m_instanceManager, &ApplicationInstanceManager::messageReceived, this, &Application::processMessage);

    if (isFileLoggerEnabled())
    {
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld()
                                      , fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    }

    Logger::instance()->addMessage(tr("qBittorrent %1 started", "qBittorrent v3.2.0alpha started").arg(QBT_VERSION));
    if (portableModeEnabled)
    {
        Logger::instance()->addMessage(tr("Running in portable mode. Auto detected profile folder at: %1").arg(profileDir.toString()));
        if (m_commandLineArgs.relativeFastresumePaths)
            Logger::instance()->addMessage(tr("Redundant command line flag detected: \"%1\". Portable mode implies relative fastresume.").arg("--relative-fastresume"), Log::WARNING); // to avoid translating the `--relative-fastresume` string
    }
    else
    {
        Logger::instance()->addMessage(tr("Using config directory: %1").arg(Profile::instance()->location(SpecialFolder::Config).toString()));
    }

#ifdef Q_OS_WIN
    applyMemoryWorkingSetLimit();
#endif
}

Application::~Application()
{
    // we still need to call cleanup()
    // in case the App failed to start
    cleanup();

    delete m_qtApp;
}

QCoreApplication *Application::qtApp() const
{
    return m_qtApp;
}

void Application::activate()
{
}

bool Application::initializeComponents()
{
    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();
    IconProvider::initInstance();

    try
    {
        BitTorrent::Session::initInstance();
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &Application::torrentFinished);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::allTorrentsFinished, this, &Application::allTorrentsFinished, Qt::QueuedConnection);

        Net::GeoIPManager::initInstance();
        TorrentFilesWatcher::initInstance();

#ifndef DISABLE_WEBUI
        m_webui = new WebUI;
#ifdef DISABLE_GUI
        if (m_webui->isErrored())
            return false;
        connect(m_webui, &WebUI::fatalError, this, []() { QCoreApplication::exit(1); });
#endif // DISABLE_GUI
#endif // DISABLE_WEBUI

        new RSS::Session; // create RSS::Session singleton
        new RSS::AutoDownloader; // create RSS::AutoDownloader singleton
    }
    catch (const RuntimeError &err)
    {
#ifdef DISABLE_GUI
        fprintf(stderr, "%s", qPrintable(err.message()));
#else
//        QMessageBox msgBox;
//        msgBox.setIcon(QMessageBox::Critical);
//        msgBox.setText(tr("Application failed to start."));
//        msgBox.setInformativeText(err.message());
//        msgBox.show(); // Need to be shown or to moveToCenter does not work
//        msgBox.move(Utils::Gui::screenCenter(&msgBox));
//        msgBox.exec();
#endif
        return false;
    }

#ifdef DISABLE_GUI
#ifndef DISABLE_WEBUI
    const Preferences *pref = Preferences::instance();

    const auto scheme = QString::fromLatin1(pref->isWebUiHttpsEnabled() ? "https" : "http");
    const auto url = QString::fromLatin1("%1://localhost:%2\n").arg(scheme, QString::number(pref->getWebUiPort()));
    const QString mesg = QString::fromLatin1("\n******** %1 ********\n").arg(tr("Information"))
            + tr("To control qBittorrent, access the WebUI at: %1").arg(url);
    printf("%s\n", qUtf8Printable(mesg));

    if (pref->getWebUIPassword() == "ARQ77eY1NUZaQsuDHbIMCA==:0WMRkYTUWVT9wVvdDtHAjU9b3b7uB8NR1Gur2hmQCvCDpm39Q+PsJRJPaCU51dEiz+dTzh8qbPsL8WkFljQYFQ==")
    {
        const QString warning = tr("The Web UI administrator username is: %1").arg(pref->getWebUiUsername()) + '\n'
                + tr("The Web UI administrator password has not been changed from the default: %1").arg("adminadmin") + '\n'
                + tr("This is a security risk, please change your password in program preferences.") + '\n';
        printf("%s", qUtf8Printable(warning));
    }
#endif // DISABLE_WEBUI
#endif // DISABLE_GUI
}

#ifdef Q_OS_WIN
int Application::memoryWorkingSetLimit() const
{
    return m_storeMemoryWorkingSetLimit.get(512);
}

void Application::setMemoryWorkingSetLimit(const int size)
{
    if (size == memoryWorkingSetLimit())
        return;

    m_storeMemoryWorkingSetLimit = size;
    applyMemoryWorkingSetLimit();
}

void Application::applyMemoryWorkingSetLimit()
{
    const int UNIT_SIZE = 1024 * 1024; // MiB
    const SIZE_T maxSize = memoryWorkingSetLimit() * UNIT_SIZE;
    const SIZE_T minSize = std::min<SIZE_T>((64 * UNIT_SIZE), (maxSize / 2));
    if (!::SetProcessWorkingSetSizeEx(::GetCurrentProcess(), minSize, maxSize, QUOTA_LIMITS_HARDWS_MAX_ENABLE))
    {
        const DWORD errorCode = ::GetLastError();
        QString message;
        LPVOID lpMsgBuf = nullptr;
        if (::FormatMessageW((FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS)
                             , nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, nullptr))
        {
            message = QString::fromWCharArray(reinterpret_cast<LPWSTR>(lpMsgBuf)).trimmed();
            ::LocalFree(lpMsgBuf);
        }
        LogMsg(tr("Failed to set physical memory (RAM) usage limit. Error code: %1. Error message: \"%2\"")
               .arg(QString::number(errorCode), message), Log::WARNING);
    }
}
#endif

bool Application::isFileLoggerEnabled() const
{
    return m_storeFileLoggerEnabled.get(true);
}

void Application::setFileLoggerEnabled(const bool value)
{
    if (value && !m_fileLogger)
    {
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld()
                                      , fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));

    }
    else if (!value)
    {
        delete m_fileLogger;
    }

    m_storeFileLoggerEnabled = value;
}

Path Application::fileLoggerPath() const
{
    return m_storeFileLoggerPath.get(specialFolderLocation(SpecialFolder::Data) / Path(u"logs"_qs));
}

void Application::setFileLoggerPath(const Path &path)
{
    if (m_fileLogger)
        m_fileLogger->changePath(path);
    m_storeFileLoggerPath = path;
}

bool Application::isFileLoggerBackup() const
{
    return m_storeFileLoggerBackup.get(true);
}

void Application::setFileLoggerBackup(const bool value)
{
    if (m_fileLogger)
        m_fileLogger->setBackup(value);
    m_storeFileLoggerBackup = value;
}

bool Application::isFileLoggerDeleteOld() const
{
    return m_storeFileLoggerDeleteOld.get(true);
}

void Application::setFileLoggerDeleteOld(const bool value)
{
    if (value && m_fileLogger)
        m_fileLogger->deleteOld(fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    m_storeFileLoggerDeleteOld = value;
}

int Application::fileLoggerMaxSize() const
{
    const int val = m_storeFileLoggerMaxSize.get(DEFAULT_FILELOG_SIZE);
    return std::min(std::max(val, MIN_FILELOG_SIZE), MAX_FILELOG_SIZE);
}

void Application::setFileLoggerMaxSize(const int bytes)
{
    const int clampedValue = std::min(std::max(bytes, MIN_FILELOG_SIZE), MAX_FILELOG_SIZE);
    if (m_fileLogger)
        m_fileLogger->setMaxSize(clampedValue);
    m_storeFileLoggerMaxSize = clampedValue;
}

int Application::fileLoggerAge() const
{
    const int val = m_storeFileLoggerAge.get(1);
    return std::min(std::max(val, 1), 365);
}

void Application::setFileLoggerAge(const int value)
{
    m_storeFileLoggerAge = std::min(std::max(value, 1), 365);
}

int Application::fileLoggerAgeType() const
{
    const int val = m_storeFileLoggerAgeType.get(1);
    return ((val < 0) || (val > 2)) ? 1 : val;
}

void Application::setFileLoggerAgeType(const int value)
{
    m_storeFileLoggerAgeType = ((value < 0) || (value > 2)) ? 1 : value;
}

const CommandLineParameters &Application::commandLineArgs() const
{
    return m_commandLineArgs;
}

bool Application::isRunning()
{
    return !m_instanceManager->isFirstInstance();
}

int Application::exec(const QStringList &params)
{
    if (!initializeComponents())
        return 1;

    m_running = true;

    BitTorrent::Session::instance()->startUpTorrents();

    m_paramsQueue = params + m_paramsQueue;
    if (!m_paramsQueue.isEmpty())
    {
        processParams(m_paramsQueue);
        m_paramsQueue.clear();
    }

    return qtApp()->exec();
}

bool Application::sendParams(const QStringList &params)
{
    return m_instanceManager->sendMessage(params.join(PARAMS_SEPARATOR));
}

Application::addTorrent(const QString &torrentSource, const BitTorrent::AddTorrentParams &torrentParams)
{
    BitTorrent::Session::instance()->addTorrent(torrentSource, torrentParams);
}

void Application::cleanup()
{
    // cleanup() can be called multiple times during shutdown. We only need it once.
    static QAtomicInt alreadyDone;
    if (!alreadyDone.testAndSetAcquire(0, 1))
        return;

#ifndef DISABLE_WEBUI
    delete m_webui;
#endif

    delete RSS::AutoDownloader::instance();
    delete RSS::Session::instance();

    TorrentFilesWatcher::freeInstance();
    BitTorrent::Session::freeInstance();
    Net::GeoIPManager::freeInstance();
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    delete m_fileLogger;
    Logger::freeInstance();
    IconProvider::freeInstance();
    SearchPluginManager::freeInstance();
    Utils::Fs::removeDirRecursively(Utils::Fs::tempPath());

    Profile::freeInstance();

    if (m_shutdownAct != ShutdownDialogAction::Exit)
    {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        Utils::Misc::shutdownComputer(m_shutdownAct);
    }
}

void Application::torrentFinished(BitTorrent::Torrent *const torrent)
{
    Preferences *const pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunEnabled())
        runExternalProgram(torrent);

    // Mail notification
    if (pref->isMailNotificationEnabled())
    {
        Logger::instance()->addMessage(tr("Torrent: %1, sending mail notification").arg(torrent->name()));
        sendNotificationEmail(torrent);
    }
}

void Application::allTorrentsFinished()
{
    Preferences *const pref = Preferences::instance();
    bool isExit = pref->shutdownqBTWhenDownloadsComplete();
    bool isShutdown = pref->shutdownWhenDownloadsComplete();
    bool isSuspend = pref->suspendWhenDownloadsComplete();
    bool isHibernate = pref->hibernateWhenDownloadsComplete();

    bool haveAction = isExit || isShutdown || isSuspend || isHibernate;
    if (!haveAction) return;

    ShutdownDialogAction action = ShutdownDialogAction::Exit;
    if (isSuspend)
        action = ShutdownDialogAction::Suspend;
    else if (isHibernate)
        action = ShutdownDialogAction::Hibernate;
    else if (isShutdown)
        action = ShutdownDialogAction::Shutdown;

    // ask confirm
    if ((action != ShutdownDialogAction::Exit) || !pref->dontConfirmAutoExit())
    {
        if (!confirmAutoExit(action))
            return;
    }

    // Actually shut down
    if (action != ShutdownDialogAction::Exit)
    {
        qDebug("Preparing for auto-shutdown because all downloads are complete!");
        // Disabling it for next time
        pref->setShutdownWhenDownloadsComplete(false);
        pref->setSuspendWhenDownloadsComplete(false);
        pref->setHibernateWhenDownloadsComplete(false);
        // Make sure preferences are synced before exiting
        m_shutdownAct = action;
    }

    qDebug("Exiting the application");
    qtApp()->exit();
}

void Application::processMessage(const QString &message)
{
    const QStringList params = message.split(PARAMS_SEPARATOR, Qt::SkipEmptyParts);
    // If Application is not running (i.e., other
    // components are not ready) store params
    if (m_running)
        processParams(params);
    else
        m_paramsQueue.append(params);
}

bool Application::eventFilter(QObject *obj, QEvent *event)
{
#ifdef Q_OS_MACOS
    if (event->type() == QEvent::FileOpen)
    {
        QString path = static_cast<QFileOpenEvent *>(event)->file();
        if (path.isEmpty())
            // Get the url instead
            path = static_cast<QFileOpenEvent *>(event)->url().toString();
        qDebug("Received a mac file open event: %s", qUtf8Printable(path));
        if (m_running)
            processParams(QStringList(path));
        else
            m_paramsQueue.append(path);
        return true;
    }
#endif // Q_OS_MACOS

    return QObject::eventFilter(obj, event);
}

void Application::initializeTranslation()
{
    Preferences *const pref = Preferences::instance();
    // Load translation
    const QString localeStr = pref->getLocale();

    if (m_qtTranslator.load(QLatin1String("qtbase_") + localeStr, QLibraryInfo::location(QLibraryInfo::TranslationsPath))
            || m_qtTranslator.load(QLatin1String("qt_") + localeStr, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qDebug("Qt %s locale recognized, using translation.", qUtf8Printable(localeStr));
    }
    else
    {
        qDebug("Qt %s locale unrecognized, using default (en).", qUtf8Printable(localeStr));
    }

    qtApp()->installTranslator(&m_qtTranslator);

    if (m_translator.load(QLatin1String(":/lang/qbittorrent_") + localeStr))
        qDebug("%s locale recognized, using translation.", qUtf8Printable(localeStr));
    else
        qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(localeStr));

    qtApp()->installTranslator(&m_translator);
}

void Application::runExternalProgram(const BitTorrent::Torrent *torrent) const
{
#if defined(Q_OS_WIN)
    const auto chopPathSep = [](const QString &str) -> QString
    {
        if (str.endsWith('\\'))
            return str.mid(0, (str.length() -1));
        return str;
    };
#endif

    QString program = Preferences::instance()->getAutoRunProgram().trimmed();

    for (int i = (program.length() - 2); i >= 0; --i)
    {
        if (program[i] != QLatin1Char('%'))
            continue;

        const ushort specifier = program[i + 1].unicode();
        switch (specifier)
        {
        case u'C':
            program.replace(i, 2, QString::number(torrent->filesCount()));
            break;
        case u'D':
#if defined(Q_OS_WIN)
            program.replace(i, 2, chopPathSep(torrent->savePath().toString()));
#else
            program.replace(i, 2, torrent->savePath().toString());
#endif
            break;
        case u'F':
#if defined(Q_OS_WIN)
            program.replace(i, 2, chopPathSep(torrent->contentPath().toString()));
#else
            program.replace(i, 2, torrent->contentPath().toString());
#endif
            break;
        case u'G':
            program.replace(i, 2, torrent->tags().join(QLatin1String(",")));
            break;
        case u'I':
            program.replace(i, 2, (torrent->infoHash().v1().isValid() ? torrent->infoHash().v1().toString() : QLatin1String("-")));
            break;
        case u'J':
            program.replace(i, 2, (torrent->infoHash().v2().isValid() ? torrent->infoHash().v2().toString() : QLatin1String("-")));
            break;
        case u'K':
            program.replace(i, 2, torrent->id().toString());
            break;
        case u'L':
            program.replace(i, 2, torrent->category());
            break;
        case u'N':
            program.replace(i, 2, torrent->name());
            break;
        case u'R':
#if defined(Q_OS_WIN)
            program.replace(i, 2, chopPathSep(torrent->rootPath().toString()));
#else
            program.replace(i, 2, torrent->rootPath().toString());
#endif
            break;
        case u'T':
            program.replace(i, 2, torrent->currentTracker());
            break;
        case u'Z':
            program.replace(i, 2, QString::number(torrent->totalSize()));
            break;
        default:
            // do nothing
            break;
        }

        // decrement `i` to avoid unwanted replacement, example pattern: "%%N"
        --i;
    }

    LogMsg(tr("Torrent: %1, running external program, command: %2").arg(torrent->name(), program));

#if defined(Q_OS_WIN)
    auto programWchar = std::make_unique<wchar_t[]>(program.length() + 1);
    program.toWCharArray(programWchar.get());

    // Need to split arguments manually because QProcess::startDetached(QString)
    // will strip off empty parameters.
    // E.g. `python.exe "1" "" "3"` will become `python.exe "1" "3"`
    int argCount = 0;
    std::unique_ptr<LPWSTR[], decltype(&::LocalFree)> args {::CommandLineToArgvW(programWchar.get(), &argCount), ::LocalFree};

    QStringList argList;
    for (int i = 1; i < argCount; ++i)
        argList += QString::fromWCharArray(args[i]);

    QProcess proc;
    proc.setProgram(QString::fromWCharArray(args[0]));
    proc.setArguments(argList);
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args)
    {
        if (Preferences::instance()->isAutoRunConsoleEnabled())
        {
            args->flags |= CREATE_NEW_CONSOLE;
            args->flags &= ~(CREATE_NO_WINDOW | DETACHED_PROCESS);
        }
        else
        {
            args->flags |= CREATE_NO_WINDOW;
            args->flags &= ~(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
        }
        args->inheritHandles = false;
        args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
        ::CloseHandle(args->startupInfo->hStdInput);
        ::CloseHandle(args->startupInfo->hStdOutput);
        ::CloseHandle(args->startupInfo->hStdError);
        args->startupInfo->hStdInput = nullptr;
        args->startupInfo->hStdOutput = nullptr;
        args->startupInfo->hStdError = nullptr;
    });
    proc.startDetached();
#else // Q_OS_WIN
    // Cannot give users shell environment by default, as doing so could
    // enable command injection via torrent name and other arguments
    // (especially when some automated download mechanism has been setup).
    // See: https://github.com/qbittorrent/qBittorrent/issues/10925
    QStringList args = QProcess::splitCommand(program);
    if (args.isEmpty())
        return;

    const QString command = args.takeFirst();
    QProcess::startDetached(command, args);
#endif
}

void Application::sendNotificationEmail(const BitTorrent::Torrent *torrent)
{
    // Prepare mail content
    const QString content = tr("Torrent name: %1").arg(torrent->name()) + '\n'
            + tr("Torrent size: %1").arg(Utils::Misc::friendlyUnit(torrent->wantedSize())) + '\n'
            + tr("Save path: %1").arg(torrent->savePath().toString()) + "\n\n"
            + tr("The torrent was downloaded in %1.", "The torrent was downloaded in 1 hour and 20 seconds")
            .arg(Utils::Misc::userFriendlyDuration(torrent->activeTime())) + "\n\n\n"
            + tr("Thank you for using qBittorrent.") + '\n';

    // Send the notification email
    const Preferences *pref = Preferences::instance();
    auto smtp = new Net::Smtp(this);
    smtp->sendMail(pref->getMailNotificationSender(),
                   pref->getMailNotificationEmail(),
                   tr("[qBittorrent] '%1' has finished downloading").arg(torrent->name()),
                   content);
}

void Application::processParams(const QStringList &params)
{
    if (params.isEmpty())
    {
        activate();
        return;
    }

    BitTorrent::AddTorrentParams torrentParams;

    for (const QString &param : params)
    {
        if (processParam(param.trimmed(), torrentParams))
            continue;

        if (param.startsWith(u"@skipDialog="))
            continue;

        addTorrent(param, torrentParams);
    }
}

bool Application::confirmAutoExit(const ShutdownDialogAction action) const
{
    return true;
}

bool Application::processParam(const QStringView param, BitTorrent::AddTorrentParams &torrentParams) const
{
    // Process strings indicating options specified by the user.

    if (param.startsWith(u"@savePath="))
    {
        torrentParams.savePath = Path(param.mid(10));
        return true;
    }

    if (param.startsWith(u"@addPaused="))
    {
        torrentParams.addPaused = (param.mid(11).toInt() != 0);
        return true;
    }

    if (param == u"@skipChecking")
    {
        torrentParams.skipChecking = true;
        return true;
    }

    if (param.startsWith(u"@category="))
    {
        torrentParams.category = param.mid(10);
        return true;
    }

    if (param == u"@sequential")
    {
        torrentParams.sequential = true;
        return true;
    }

    if (param == u"@firstLastPiecePriority")
    {
        torrentParams.firstLastPiecePriority = true;
        return true;
    }


    return false;
}

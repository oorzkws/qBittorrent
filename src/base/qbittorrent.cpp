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

#include "qbittorrent.h"

#include <QtGlobal>

#include <algorithm>
#include <cstdio>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#include <memory>
#include <windows.h>
#include <shellapi.h>
#endif

#include <QAtomicInt>
#include <QDebug>
#include <QLibraryInfo>
#include <QProcess>
#include <QTranslator>

#include "applicationinstancemanager.h"
#include "bittorrent/session.h"
#include "bittorrent/torrenthandle.h"
#include "exceptions.h"
#include "iconprovider.h"
#include "logger.h"
#include "net/downloadmanager.h"
#include "net/geoipmanager.h"
#include "net/proxyconfigurationmanager.h"
#include "net/smtp.h"
#include "preferences.h"
#include "profile.h"
#include "rss/rss_autodownloader.h"
#include "rss/rss_session.h"
#include "scanfoldersmodel.h"
#include "search/searchpluginmanager.h"
#include "settingsstorage.h"
#include "utils/fs.h"
#include "utils/misc.h"
#include "utils/string.h"
#include "filelogger.h"

#ifndef DISABLE_WEBUI
#include "webui/webui.h"
#endif

namespace
{
#define SETTINGS_KEY(name) "Application/" name

    // FileLogger properties keys
#define FILELOGGER_SETTINGS_KEY(name) QStringLiteral(SETTINGS_KEY("FileLogger/") name)
    const QString KEY_FILELOGGER_ENABLED = FILELOGGER_SETTINGS_KEY("Enabled");
    const QString KEY_FILELOGGER_PATH = FILELOGGER_SETTINGS_KEY("Path");
    const QString KEY_FILELOGGER_BACKUP = FILELOGGER_SETTINGS_KEY("Backup");
    const QString KEY_FILELOGGER_DELETEOLD = FILELOGGER_SETTINGS_KEY("DeleteOld");
    const QString KEY_FILELOGGER_MAXSIZEBYTES = FILELOGGER_SETTINGS_KEY("MaxSizeBytes");
    const QString KEY_FILELOGGER_AGE = FILELOGGER_SETTINGS_KEY("Age");
    const QString KEY_FILELOGGER_AGETYPE = FILELOGGER_SETTINGS_KEY("AgeType");

    // just a shortcut
    inline SettingsStorage *settings() { return  SettingsStorage::instance(); }

    const QString LOG_FOLDER = QStringLiteral("logs");
    const QChar PARAMS_SEPARATOR = '|';

    const QString DEFAULT_PORTABLE_MODE_PROFILE_DIR = QStringLiteral("profile");

    const int MIN_FILELOG_SIZE = 1024; // 1KiB
    const int MAX_FILELOG_SIZE = 1000 * 1024 * 1024; // 1000MiB
    const int DEFAULT_FILELOG_SIZE = 65 * 1024; // 65KiB
}

QBittorrent::QBittorrent(QCoreApplication &app)
    : m_qtTranslator {new QTranslator {this}}
    , m_translator {new QTranslator {this}}
    , m_commandLineParameters {CommandLineParameters::parse(app.arguments())}
{
    qRegisterMetaType<Log::Msg>("Log::Msg");

    m_paramsQueue += m_commandLineParameters.paramList();

    m_profile = new Profile {m_commandLineParameters.profileDir, m_commandLineParameters.configurationName
                , (m_commandLineParameters.relativeFastresumePaths || m_commandLineParameters.portableMode)};

    Logger::initInstance();
    SettingsStorage::initInstance();
    Preferences::initInstance();

    if (m_commandLineParameters.webUiPort > 0) // it will be -1 when user did not set any value
        Preferences::instance()->setWebUiPort(static_cast<quint16>(m_commandLineParameters.webUiPort));

    initializeTranslation();

    connect(&app, &QCoreApplication::aboutToQuit, this, &QBittorrent::cleanup);

    if (isFileLoggerEnabled())
        m_fileLogger = new FileLogger {fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize()
                       , isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType())};
}

QBittorrent::~QBittorrent()
{
    // we still need to call cleanup()
    // in case the App failed to start
    cleanup();
}

bool QBittorrent::isFileLoggerEnabled() const
{
    return settings()->loadValue(KEY_FILELOGGER_ENABLED, true).toBool();
}

void QBittorrent::setFileLoggerEnabled(const bool value)
{
    if (value && !m_fileLogger)
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    else if (!value)
        delete m_fileLogger;
    settings()->storeValue(KEY_FILELOGGER_ENABLED, value);
}

QString QBittorrent::fileLoggerPath() const
{
    return settings()->loadValue(KEY_FILELOGGER_PATH,
            QVariant(m_profile->location(SpecialFolder::Data) + LOG_FOLDER)).toString();
}

void QBittorrent::setFileLoggerPath(const QString &path)
{
    if (m_fileLogger)
        m_fileLogger->changePath(path);
    settings()->storeValue(KEY_FILELOGGER_PATH, path);
}

bool QBittorrent::isFileLoggerBackup() const
{
    return settings()->loadValue(KEY_FILELOGGER_BACKUP, true).toBool();
}

void QBittorrent::setFileLoggerBackup(const bool value)
{
    if (m_fileLogger)
        m_fileLogger->setBackup(value);
    settings()->storeValue(KEY_FILELOGGER_BACKUP, value);
}

bool QBittorrent::isFileLoggerDeleteOld() const
{
    return settings()->loadValue(KEY_FILELOGGER_DELETEOLD, true).toBool();
}

void QBittorrent::setFileLoggerDeleteOld(const bool value)
{
    if (value && m_fileLogger)
        m_fileLogger->deleteOld(fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    settings()->storeValue(KEY_FILELOGGER_DELETEOLD, value);
}

int QBittorrent::fileLoggerMaxSize() const
{
    const int val = settings()->loadValue(KEY_FILELOGGER_MAXSIZEBYTES, DEFAULT_FILELOG_SIZE).toInt();
    return std::min(std::max(val, MIN_FILELOG_SIZE), MAX_FILELOG_SIZE);
}

void QBittorrent::setFileLoggerMaxSize(const int bytes)
{
    const int clampedValue = std::min(std::max(bytes, MIN_FILELOG_SIZE), MAX_FILELOG_SIZE);
    if (m_fileLogger)
        m_fileLogger->setMaxSize(clampedValue);
    settings()->storeValue(KEY_FILELOGGER_MAXSIZEBYTES, clampedValue);
}

int QBittorrent::fileLoggerAge() const
{
    const int val = settings()->loadValue(KEY_FILELOGGER_AGE, 1).toInt();
    return std::min(std::max(val, 1), 365);
}

void QBittorrent::setFileLoggerAge(const int value)
{
    settings()->storeValue(KEY_FILELOGGER_AGE, std::min(std::max(value, 1), 365));
}

int QBittorrent::fileLoggerAgeType() const
{
    const int val = settings()->loadValue(KEY_FILELOGGER_AGETYPE, 1).toInt();
    return ((val < 0) || (val > 2)) ? 1 : val;
}

void QBittorrent::setFileLoggerAgeType(const int value)
{
    settings()->storeValue(KEY_FILELOGGER_AGETYPE, ((value < 0) || (value > 2)) ? 1 : value);
}

bool QBittorrent::createComponents()
{
    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();

    m_iconProvider = createIconProvider();

    try {
        BitTorrent::Session::initInstance();
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &QBittorrent::torrentFinished);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::allTorrentsFinished, this, &QBittorrent::allTorrentsFinished, Qt::QueuedConnection);

#ifndef DISABLE_COUNTRIES_RESOLUTION
        Net::GeoIPManager::initInstance();
#endif
        ScanFoldersModel::initInstance(this);

#ifndef DISABLE_WEBUI
        m_webui = createWebUI();
#endif // DISABLE_WEBUI

        new RSS::Session; // create RSS::Session singleton
        new RSS::AutoDownloader; // create RSS::AutoDownloader singleton
    }
    catch (const RuntimeError &err) {
        showErrorMessage(err.message());
        return false;
    }

    return true;
}

void QBittorrent::beginCleanup()
{
}

void QBittorrent::endCleanup()
{
}

CommandLineParameters QBittorrent::commandLineParameters() const
{
    return m_commandLineParameters;
}

bool QBittorrent::confirmShutdown() const
{
    return true;
}

void QBittorrent::showStartupInfo() const
{
#ifndef DISABLE_WEBUI
    Preferences *const pref = Preferences::instance();
    // Display some information to the user
    const QString mesg = QString("\n******** %1 ********\n").arg(tr("Information"))
        + tr("To control qBittorrent, access the Web UI at %1")
            .arg(QString("http://localhost:") + QString::number(pref->getWebUiPort())) + '\n';
    printf("%s", qUtf8Printable(mesg));

    if (pref->getWebUIPassword() == "ARQ77eY1NUZaQsuDHbIMCA==:0WMRkYTUWVT9wVvdDtHAjU9b3b7uB8NR1Gur2hmQCvCDpm39Q+PsJRJPaCU51dEiz+dTzh8qbPsL8WkFljQYFQ==") {
        const QString warning = tr("The Web UI administrator username is: %1").arg(pref->getWebUiUsername()) + '\n'
            + tr("The Web UI administrator password is still the default one: %1").arg("adminadmin") + '\n'
            + tr("This is a security risk, please consider changing your password from program preferences.") + '\n';
        printf("%s", qUtf8Printable(warning));
    }
#endif // DISABLE_WEBUI
}

void QBittorrent::showErrorMessage(const QString &message) const
{
    fprintf(stderr, "%s", message.toUtf8().constData());
}

void QBittorrent::activate() const
{
    // does nothing by default
}

bool QBittorrent::userAgreesWithLegalNotice()
{
#ifdef HAS_DAEMON_MODE
    if (m_commandLineParameters.shouldDaemonize)
        return true;
#endif
#ifdef Q_OS_WIN
    if (!_isatty(_fileno(stdin)) || !_isatty(_fileno(stdout)))
#else
    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout)))
#endif
        return true;

    const QString eula = QString("\n*** %1 ***\n").arg(tr("Legal Notice"))
        + tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.") + "\n\n"
        + tr("No further notices will be issued.") + "\n\n"
        + tr("Press %1 key to accept and continue...").arg("'y'") + '\n';
    printf("%s", qUtf8Printable(eula));

    char ret = static_cast<char>(getchar()); // Read pressed key
    if ((ret == 'y') || (ret == 'Y')) {
        // Save the answer
        Preferences::instance()->setAcceptedLegal(true);
        return true;
    }

    return false;
}

void QBittorrent::displayUsage() const
{
    printf("%s\n", qUtf8Printable(m_commandLineParameters.makeUsage()));
}

void QBittorrent::displayVersion() const
{
    printf("%s %s\n", qUtf8Printable(QCoreApplication::applicationName()), QBT_VERSION);
}

IconProvider *QBittorrent::createIconProvider()
{
    return new IconProvider {this};
}

#ifndef DISABLE_WEBUI
WebUI *QBittorrent::createWebUI()
{
    auto *webUI = new WebUI;
    if (webUI->isErrored())
        throw RuntimeError {tr("Failed to initialize Web Access component.")};
    connect(webUI, &WebUI::fatalError, this, []() { QCoreApplication::exit(1); });

    return webUI;
}
#endif

void QBittorrent::processMessage(const QString &message)
{
    const QStringList params = message.split(PARAMS_SEPARATOR, QString::SkipEmptyParts);
    // If Application is not running (i.e., other
    // components are not ready) store params
    if (m_running)
        processParams(params);
    else
        m_paramsQueue.append(params);
}

void QBittorrent::runExternalProgram(const BitTorrent::TorrentHandle *torrent) const
{
    QString program = Preferences::instance()->getAutoRunProgram().trimmed();
    program.replace("%N", torrent->name());
    program.replace("%L", torrent->category());

    QStringList tags = torrent->tags().toList();
    std::sort(tags.begin(), tags.end(), Utils::String::naturalLessThan<Qt::CaseInsensitive>);
    program.replace("%G", tags.join(','));

#if defined(Q_OS_WIN)
    const auto chopPathSep = [](const QString &str) -> QString
    {
        if (str.endsWith('\\'))
            return str.mid(0, (str.length() -1));
        return str;
    };
    program.replace("%F", chopPathSep(Utils::Fs::toNativePath(torrent->contentPath())));
    program.replace("%R", chopPathSep(Utils::Fs::toNativePath(torrent->rootPath())));
    program.replace("%D", chopPathSep(Utils::Fs::toNativePath(torrent->savePath())));
#else
    program.replace("%F", Utils::Fs::toNativePath(torrent->contentPath()));
    program.replace("%R", Utils::Fs::toNativePath(torrent->rootPath()));
    program.replace("%D", Utils::Fs::toNativePath(torrent->savePath()));
#endif
    program.replace("%C", QString::number(torrent->filesCount()));
    program.replace("%Z", QString::number(torrent->totalSize()));
    program.replace("%T", torrent->currentTracker());
    program.replace("%I", torrent->hash());

    Logger *logger = Logger::instance();
    logger->addMessage(tr("Torrent: %1, running external program, command: %2").arg(torrent->name(), program));

#if defined(Q_OS_WIN)
    std::unique_ptr<wchar_t[]> programWchar(new wchar_t[program.length() + 1] {});
    program.toWCharArray(programWchar.get());

    // Need to split arguments manually because QProcess::startDetached(QString)
    // will strip off empty parameters.
    // E.g. `python.exe "1" "" "3"` will become `python.exe "1" "3"`
    int argCount = 0;
    LPWSTR *args = ::CommandLineToArgvW(programWchar.get(), &argCount);

    QStringList argList;
    for (int i = 1; i < argCount; ++i)
        argList += QString::fromWCharArray(args[i]);

    QProcess::startDetached(QString::fromWCharArray(args[0]), argList);

    ::LocalFree(args);
#else
    QProcess::startDetached(QLatin1String("/bin/sh"), {QLatin1String("-c"), program});
#endif
}

void QBittorrent::sendNotificationEmail(const BitTorrent::TorrentHandle *torrent)
{
    // Prepare mail content
    const QString content = tr("Torrent name: %1").arg(torrent->name()) + '\n'
        + tr("Torrent size: %1").arg(Utils::Misc::friendlyUnit(torrent->wantedSize())) + '\n'
        + tr("Save path: %1").arg(torrent->savePath()) + "\n\n"
        + tr("The torrent was downloaded in %1.", "The torrent was downloaded in 1 hour and 20 seconds")
            .arg(Utils::Misc::userFriendlyDuration(torrent->activeTime())) + "\n\n\n"
        + tr("Thank you for using qBittorrent.") + '\n';

    // Send the notification email
    const Preferences *pref = Preferences::instance();
    auto smtp = new Net::Smtp {this};
    smtp->sendMail(pref->getMailNotificationSender(), pref->getMailNotificationEmail()
                   , tr("[qBittorrent] '%1' has finished downloading").arg(torrent->name())
                   , content);
}

void QBittorrent::torrentFinished(BitTorrent::TorrentHandle *const torrent)
{
    Preferences *const pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunEnabled())
        runExternalProgram(torrent);

    // Mail notification
    if (pref->isMailNotificationEnabled()) {
        Logger::instance()->addMessage(tr("Torrent: %1, sending mail notification").arg(torrent->name()));
        sendNotificationEmail(torrent);
    }
}

void QBittorrent::allTorrentsFinished()
{
    Preferences *const pref = Preferences::instance();
    bool isExit = pref->shutdownqBTWhenDownloadsComplete();
    bool isShutdown = pref->shutdownWhenDownloadsComplete();
    bool isSuspend = pref->suspendWhenDownloadsComplete();
    bool isHibernate = pref->hibernateWhenDownloadsComplete();

    bool haveAction = isExit || isShutdown || isSuspend || isHibernate;
    if (!haveAction) return;

    ShutdownAction action = ShutdownAction::Exit;
    if (isSuspend)
        action = ShutdownAction::Suspend;
    else if (isHibernate)
        action = ShutdownAction::Hibernate;
    else if (isShutdown)
        action = ShutdownAction::Shutdown;

    // ask confirm
    if ((action == ShutdownAction::Exit) && (pref->dontConfirmAutoExit())) {
        // do nothing & skip confirm
    }
    else {
        if (!confirmShutdown()) return;
    }

    // Actually shut down
    if (action != ShutdownAction::Exit) {
        qDebug("Preparing for auto-shutdown because all downloads are complete!");
        // Disabling it for next time
        pref->setShutdownWhenDownloadsComplete(false);
        pref->setSuspendWhenDownloadsComplete(false);
        pref->setHibernateWhenDownloadsComplete(false);
        // Make sure preferences are synced before exiting
        m_shutdownAction = action;
    }

    qDebug("Exiting the application");
    QCoreApplication::exit();
}

const Profile *QBittorrent::profile() const
{
    return m_profile;
}

const IconProvider *QBittorrent::iconProvider() const
{
    return m_iconProvider;
}

void QBittorrent::addTorrent(const QString &source, const BitTorrent::AddTorrentParams &torrentParams)
{
    BitTorrent::Session::instance()->addTorrent(source, torrentParams);
}

// As program parameters, we can get paths or urls.
// This function parse the parameters and call
// the right addTorrent function, considering
// the parameter type.
void QBittorrent::processParams(const QStringList &params)
{
    if (params.isEmpty()) {
        activate();
        return;
    }

    BitTorrent::AddTorrentParams torrentParams;
    TriStateBool skipTorrentDialog;

    for (QString param : params) {
        param = param.trimmed();

        // Process strings indicating options specified by the user.

        if (param.startsWith(QLatin1String("@savePath="))) {
            torrentParams.savePath = param.mid(10);
            continue;
        }

        if (param.startsWith(QLatin1String("@addPaused="))) {
            torrentParams.addPaused = param.midRef(11).toInt() ? TriStateBool::True : TriStateBool::False;
            continue;
        }

        if (param == QLatin1String("@skipChecking")) {
            torrentParams.skipChecking = true;
            continue;
        }

        if (param.startsWith(QLatin1String("@category="))) {
            torrentParams.category = param.mid(10);
            continue;
        }

        if (param == QLatin1String("@sequential")) {
            torrentParams.sequential = true;
            continue;
        }

        if (param == QLatin1String("@firstLastPiecePriority")) {
            torrentParams.firstLastPiecePriority = true;
            continue;
        }

        if (param.startsWith(QLatin1String("@skipDialog="))) {
            skipTorrentDialog = param.midRef(12).toInt() ? TriStateBool::True : TriStateBool::False;
            continue;
        }

        addTorrent(param, torrentParams);
    }
}

int QBittorrent::run()
{
#ifndef Q_OS_WIN
    if (m_commandLineParameters.showVersion) {
        displayVersion();
        return EXIT_SUCCESS;
    }
#endif
    if (m_commandLineParameters.showHelp) {
        displayUsage();
        return EXIT_SUCCESS;
    }

    if (!Preferences::instance()->getAcceptedLegal() && !userAgreesWithLegalNotice())
        return EXIT_SUCCESS;

    // Set environment variable
    if (!qputenv("QBITTORRENT", QBT_VERSION))
        fprintf(stderr, "Couldn't set environment variable...\n");

    if (!createComponents())
        return EXIT_FAILURE;

    const QString appId = QLatin1String("qBittorrent-") + Utils::Misc::getUserIDString();
    std::unique_ptr<ApplicationInstanceManager> instanceManager = std::make_unique<ApplicationInstanceManager>(appId, this);
    connect(&*instanceManager, &ApplicationInstanceManager::messageReceived, this, &QBittorrent::processMessage);

//    instanceManager->sendMessage(params.join(PARAMS_SEPARATOR))

    showStartupInfo();

    BitTorrent::Session::instance()->startUpTorrents();

    if (!m_paramsQueue.isEmpty()) {
        processParams(m_paramsQueue);
        m_paramsQueue.clear();
    }

    m_running = true;
    LogMsg(tr("qBittorrent %1 started", "qBittorrent v3.2.0alpha started").arg(QBT_VERSION));

    return QCoreApplication::exec();
}

void QBittorrent::cleanup()
{
    // cleanup() can be called multiple times during shutdown. We only need it once.
    static QAtomicInt alreadyDone;
    if (!alreadyDone.testAndSetAcquire(0, 1))
        return;

    beginCleanup();

#ifndef DISABLE_WEBUI
    delete m_webui;
#endif

    delete RSS::AutoDownloader::instance();
    delete RSS::Session::instance();

    ScanFoldersModel::freeInstance();
    BitTorrent::Session::freeInstance();
#ifndef DISABLE_COUNTRIES_RESOLUTION
    Net::GeoIPManager::freeInstance();
#endif
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    delete m_fileLogger;
    Logger::freeInstance();
    delete m_iconProvider;
    SearchPluginManager::freeInstance();
    Utils::Fs::removeDirRecursive(Utils::Fs::tempPath());

    endCleanup();

    if (m_shutdownAction != ShutdownAction::Exit) {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        Utils::Misc::shutdownComputer(m_shutdownAction);
    }
}

void QBittorrent::initializeTranslation()
{
    // Load translation
    const QString localeStr = Preferences::instance()->getLocale();

    if (m_qtTranslator->load(QLatin1String("qtbase_") + localeStr, QLibraryInfo::location(QLibraryInfo::TranslationsPath)) ||
            m_qtTranslator->load(QLatin1String("qt_") + localeStr, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        qDebug("Qt %s locale recognized, using translation.", qUtf8Printable(localeStr));
    else
        qDebug("Qt %s locale unrecognized, using default (en).", qUtf8Printable(localeStr));

    QCoreApplication::installTranslator(m_qtTranslator);

    if (m_translator->load(QLatin1String(":/lang/qbittorrent_") + localeStr))
        qDebug("%s locale recognized, using translation.", qUtf8Printable(localeStr));
    else
        qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(localeStr));
    QCoreApplication::installTranslator(m_translator);
}

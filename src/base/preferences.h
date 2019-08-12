/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
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

#pragma once

#include <QList>

#include "net/proxytype.h"
#include "preferencesbase.h"
#include "utils/net.h"

class QDateTime;
class QNetworkCookie;
class QSize;
class QStringList;
class QTime;
class QVariant;

enum SchedulerDays
{
    EVERY_DAY,
    WEEK_DAYS,
    WEEK_ENDS,
    MON,
    TUE,
    WED,
    THU,
    FRI,
    SAT,
    SUN
};

namespace TrayIcon
{
    enum Style
    {
        NORMAL = 0,
        MONO_DARK,
        MONO_LIGHT
    };
}

namespace DNS
{
    enum Service
    {
        DYNDNS,
        NOIP,
        NONE = -1
    };
}

template <typename T>
ProxyFunc<T> lowerLimited(T limit, T ret)
{
    return [limit, ret](const T &value) -> T
    {
        return value <= limit ? ret : value;
    };
}

template <typename T>
ProxyFunc<T> lowerLimited(T limit)
{
    return lowerLimited(limit, limit);
}

template <typename T>
ProxyFunc<T> clampValue(const T lower, const T upper)
{
    // TODO: change return type to `auto` when using C++17
    return [lower, upper](const T value) -> T
    {
        if (value < lower)
            return lower;
        if (value > upper)
            return upper;
        return value;
    };
}

class Preferences : public PreferencesBase
{
    Q_OBJECT
    Q_DISABLE_COPY(Preferences)

    Preferences() = default;

    static Preferences *m_instance;

public:
    static void initInstance();
    static void freeInstance();
    static Preferences *instance();

    static constexpr int ADDTORRENTDIALOG_MINPATHHISTORYLENGTH = 0;
    static constexpr int ADDTORRENTDIALOG_MAXPATHHISTORYLENGTH = 99;

    // GUI options
    bool isAddTorrentDialogEnabled() const;
    void setAddTorrentDialogEnabled(bool value);
    bool isAddTorrentDialogTopLevel() const;
    void setAddTorrentDialogTopLevel(bool value);
    int addTorrentDialogSavePathHistoryLength() const;
    void setAddTorrentDialogSavePathHistoryLength(int value);
    bool useCustomUITheme() const;
    void setUseCustomUITheme(bool use);
    QString customUIThemePath() const;
    void setCustomUIThemePath(const QString &path);
    bool confirmOnExit() const;
    void setConfirmOnExit(bool confirm);
    bool speedInTitleBar() const;
    void showSpeedInTitleBar(bool show);
    bool useAlternatingRowColors() const;
    void setAlternatingRowColors(bool b);
    bool getHideZeroValues() const;
    void setHideZeroValues(bool b);
    int getHideZeroComboValues() const;
    void setHideZeroComboValues(int n);
    bool isStatusbarDisplayed() const;
    void setStatusbarDisplayed(bool displayed);
    bool isToolbarDisplayed() const;
    void setToolbarDisplayed(bool displayed);
    bool startMinimized() const;
    void setStartMinimized(bool b);
    bool isSplashScreenDisabled() const;
    void setSplashScreenDisabled(bool b);
    int getActionOnDblClOnTorrentDl() const;
    void setActionOnDblClOnTorrentDl(int act);
    int getActionOnDblClOnTorrentFn() const;
    void setActionOnDblClOnTorrentFn(int act);
    QString getScanDirsLastPath() const;
    void setScanDirsLastPath(const QString &path);
    QByteArray getUILockPassword() const;
    void setUILockPassword(const QByteArray &password);
    bool isUILocked() const;
    void setUILocked(bool locked);
    bool dontConfirmAutoExit() const;
    void setDontConfirmAutoExit(bool dontConfirmAutoExit);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    bool useSystemIconTheme() const;
    void useSystemIconTheme(bool enabled);
#endif
    bool confirmTorrentDeletion() const;
    void setConfirmTorrentDeletion(bool enabled);
    bool confirmTorrentRecheck() const;
    void setConfirmTorrentRecheck(bool enabled);
    bool confirmRemoveAllTags() const;
    void setConfirmRemoveAllTags(bool enabled);
#ifndef Q_OS_MACOS
    bool systrayIntegration() const;
    void setSystrayIntegration(bool enabled);
    bool minimizeToTrayNotified() const;
    void setMinimizeToTrayNotified(bool b);
    bool minimizeToTray() const;
    void setMinimizeToTray(bool b);
    bool closeToTray() const;
    void setCloseToTray(bool b);
    bool closeToTrayNotified() const;
    void setCloseToTrayNotified(bool b);
    TrayIcon::Style trayIconStyle() const;
    void setTrayIconStyle(TrayIcon::Style style);
#endif // Q_OS_MACOS
    bool isRSSWidgetEnabled() const;
    void setRSSWidgetVisible(bool enabled);
    bool isSpeedWidgetEnabled() const;
    void setSpeedWidgetEnabled(bool enabled);
    int getSpeedWidgetPeriod() const;
    void setSpeedWidgetPeriod(int period);
    bool getSpeedWidgetGraphEnable(int id) const;
    void setSpeedWidgetGraphEnable(int id, bool enable);
    bool isDownloadTrackerFavicon() const;
    void setDownloadTrackerFavicon(bool value);
    // LogWidget
    bool isLogWidgetEnabled() const;
    void setLogWidgetEnabled(bool value);
    int logWidgetMsgTypes() const;
    void setLogWidgetMsgTypes(int value);
    // Notifications
    bool isNotificationsEnabled() const;
    void setNotificationsEnabled(bool value);
    bool isTorrentAddedNotificationsEnabled() const;
    void setTorrentAddedNotificationsEnabled(bool value);

    // General options
    // FileLogger properties
    bool isFileLoggerEnabled() const;
    void setFileLoggerEnabled(bool value);
    QString fileLoggerPath() const;
    void setFileLoggerPath(const QString &path);
    bool isFileLoggerBackup() const;
    void setFileLoggerBackup(bool value);
    bool isFileLoggerDeleteOld() const;
    void setFileLoggerDeleteOld(bool value);
    int fileLoggerMaxSize() const;
    void setFileLoggerMaxSize(int bytes);
    int fileLoggerAge() const;
    void setFileLoggerAge(int value);
    int fileLoggerAgeType() const;
    void setFileLoggerAgeType(int value);

    bool isRSSProcessingEnabled() const;
    void setRSSProcessingEnabled(bool value);
    int getRSSRefreshInterval() const;
    void setRSSRefreshInterval(int value);
    int getRSSMaxArticlesPerFeed() const;
    void setRSSMaxArticlesPerFeed(int value);
    bool isRSSAutoDownloadingEnabled() const;
    void setRSSAutoDownloadingEnabled(bool value);
    QStringList getRSSSmartEpisodeFilters() const;
    void setRSSSmartEpisodeFilters(const QStringList &value);
    bool getRSSDownloadRepacks() const;
    void setRSSDownloadRepacks(bool value);

    bool isPortForwardingEnabled() const;
    void setPortForwardingEnabled(bool value);
    Net::ProxyType proxyType() const;
    void setProxyType(Net::ProxyType value);
    QString proxyIP() const;
    void setProxyIP(const QString &value);
    int proxyPort() const;
    void setProxyPort(int value);
    QString proxyUsername() const;
    void setProxyUsername(const QString &value);
    QString proxyPassword() const;
    void setProxyPassword(const QString &value);
    bool isProxyOnlyForTorrents() const;
    void setProxyOnlyForTorrents(bool value);

    QString getLocale() const;
    void setLocale(const QString &locale);
    bool deleteTorrentFilesAsDefault() const;
    void setDeleteTorrentFilesAsDefault(bool del);
    bool preventFromSuspendWhenDownloading() const;
    void setPreventFromSuspendWhenDownloading(bool b);
    bool preventFromSuspendWhenSeeding() const;
    void setPreventFromSuspendWhenSeeding(bool b);
#ifdef Q_OS_WIN
    bool WinStartup() const;
    void setWinStartup(bool b);
#endif
    QVariantHash getScanDirs() const;
    void setScanDirs(const QVariantHash &dirs);
    bool isMailNotificationEnabled() const;
    void setMailNotificationEnabled(bool enabled);
    QString getMailNotificationSender() const;
    void setMailNotificationSender(const QString &mail);
    QString getMailNotificationEmail() const;
    void setMailNotificationEmail(const QString &mail);
    QString getMailNotificationSMTP() const;
    void setMailNotificationSMTP(const QString &smtp_server);
    bool getMailNotificationSMTPSSL() const;
    void setMailNotificationSMTPSSL(bool use);
    bool getMailNotificationSMTPAuth() const;
    void setMailNotificationSMTPAuth(bool use);
    QString getMailNotificationSMTPUsername() const;
    void setMailNotificationSMTPUsername(const QString &username);
    QString getMailNotificationSMTPPassword() const;
    void setMailNotificationSMTPPassword(const QString &password);
    QTime getSchedulerStartTime() const;
    void setSchedulerStartTime(const QTime &time);
    QTime getSchedulerEndTime() const;
    void setSchedulerEndTime(const QTime &time);
    SchedulerDays getSchedulerDays() const;
    void setSchedulerDays(SchedulerDays days);

    // Search
    bool isSearchEnabled() const;
    void setSearchEnabled(bool enabled);

    // WebUI
    bool isWebUiEnabled() const;
    void setWebUiEnabled(bool enabled);
    QString getServerDomains() const;
    void setServerDomains(const QString &str);
    QString getWebUiAddress() const;
    void setWebUiAddress(const QString &addr);
    quint16 getWebUiPort() const;
    void setWebUiPort(quint16 port);
    bool useUPnPForWebUIPort() const;
    void setUPnPForWebUIPort(bool enabled);
    bool isWebUiLocalAuthEnabled() const;
    void setWebUiLocalAuthEnabled(bool enabled);
    bool isWebUiAuthSubnetWhitelistEnabled() const;
    void setWebUiAuthSubnetWhitelistEnabled(bool enabled);
    QVector<Utils::Net::Subnet> getWebUiAuthSubnetWhitelist() const;
    void setWebUiAuthSubnetWhitelist(QStringList subnets);
    QString getWebUiUsername() const;
    void setWebUiUsername(const QString &username);
    QByteArray getWebUIPassword() const;
    void setWebUIPassword(const QByteArray &password);
    int getWebUIMaxAuthFailCount() const;
    void setWebUIMaxAuthFailCount(int count);
    std::chrono::seconds getWebUIBanDuration() const;
    void setWebUIBanDuration(std::chrono::seconds duration);
    int getWebUISessionTimeout() const;
    void setWebUISessionTimeout(int timeout);
    bool isWebUiClickjackingProtectionEnabled() const;
    void setWebUiClickjackingProtectionEnabled(bool enabled);
    bool isWebUiCSRFProtectionEnabled() const;
    void setWebUiCSRFProtectionEnabled(bool enabled);
    bool isWebUiSecureCookieEnabled () const;
    void setWebUiSecureCookieEnabled(bool enabled);
    bool isWebUIHostHeaderValidationEnabled() const;
    void setWebUIHostHeaderValidationEnabled(bool enabled);
    bool isWebUiHttpsEnabled() const;
    void setWebUiHttpsEnabled(bool enabled);
    QString getWebUIHttpsCertificatePath() const;
    void setWebUIHttpsCertificatePath(const QString &path);
    QString getWebUIHttpsKeyPath() const;
    void setWebUIHttpsKeyPath(const QString &path);
    bool isAltWebUiEnabled() const;
    void setAltWebUiEnabled(bool enabled);
    QString getWebUiRootFolder() const;
    void setWebUiRootFolder(const QString &path);

    // Dynamic DNS
    bool isDynDNSEnabled() const;
    void setDynDNSEnabled(bool enabled);
    DNS::Service getDynDNSService() const;
    void setDynDNSService(int service);
    QString getDynDomainName() const;
    void setDynDomainName(const QString &name);
    QString getDynDNSUsername() const;
    void setDynDNSUsername(const QString &username);
    QString getDynDNSPassword() const;
    void setDynDNSPassword(const QString &password);

    bool isAutoRunEnabled() const;
    void setAutoRunEnabled(bool enabled);
    QString getAutoRunProgram() const;
    void setAutoRunProgram(const QString &program);
#if defined(Q_OS_WIN) && (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    bool isAutoRunConsoleEnabled() const;
    void setAutoRunConsoleEnabled(bool enabled);
#endif

    bool shutdownWhenDownloadsComplete() const;
    void setShutdownWhenDownloadsComplete(bool shutdown);
    bool suspendWhenDownloadsComplete() const;
    void setSuspendWhenDownloadsComplete(bool suspend);
    bool hibernateWhenDownloadsComplete() const;
    void setHibernateWhenDownloadsComplete(bool hibernate);
    bool shutdownqBTWhenDownloadsComplete() const;
    void setShutdownqBTWhenDownloadsComplete(bool shutdown);

    bool recheckTorrentsOnCompletion() const;
    void recheckTorrentsOnCompletion(bool recheck);
    bool resolvePeerCountries() const;
    void resolvePeerCountries(bool resolve);
    bool resolvePeerHostNames() const;
    void resolvePeerHostNames(bool resolve);
    bool recursiveDownloadDisabled() const;
    void disableRecursiveDownload(bool disable = true);

#ifdef Q_OS_WIN
    bool neverCheckFileAssoc() const;
    void setNeverCheckFileAssoc(bool check = true);
    static bool isTorrentFileAssocSet();
    static bool isMagnetLinkAssocSet();
    static void setTorrentFileAssoc(bool set);
    static void setMagnetLinkAssoc(bool set);
#endif
#ifdef Q_OS_MACOS
    static bool isTorrentFileAssocSet();
    static bool isMagnetLinkAssocSet();
    static void setTorrentFileAssoc();
    static void setMagnetLinkAssoc();
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    bool isUpdateCheckEnabled() const;
    void setUpdateCheckEnabled(bool enabled);
#endif

    int getTrackerPort() const;
    void setTrackerPort(int port);

    // States
    QDateTime getDNSLastUpd() const;
    void setDNSLastUpd(const QDateTime &date);
    QString getDNSLastIP() const;
    void setDNSLastIP(const QString &ip);
    bool getAcceptedLegal() const;
    void setAcceptedLegal(bool accepted);
    QByteArray getMainGeometry() const;
    void setMainGeometry(const QByteArray &geometry);
    QByteArray getMainVSplitterState() const;
    void setMainVSplitterState(const QByteArray &state);
    QString getMainLastDir() const;
    void setMainLastDir(const QString &path);
    QSize getPrefSize() const;
    void setPrefSize(const QSize &size);
    QStringList getPrefHSplitterSizes() const;
    void setPrefHSplitterSizes(const QStringList &sizes);
    QByteArray getPeerListState() const;
    void setPeerListState(const QByteArray &state);
    QString getPropSplitterSizes() const;
    void setPropSplitterSizes(const QString &sizes);
    QByteArray getPropFileListState() const;
    void setPropFileListState(const QByteArray &state);
    int getPropCurTab() const;
    void setPropCurTab(int tab);
    bool getPropVisible() const;
    void setPropVisible(bool visible);
    QByteArray getPropTrackerListState() const;
    void setPropTrackerListState(const QByteArray &state);
    QSize getRssGeometrySize() const;
    void setRssGeometrySize(const QSize &geometry);
    QByteArray getRssHSplitterSizes() const;
    void setRssHSplitterSizes(const QByteArray &sizes);
    QStringList getRssOpenFolders() const;
    void setRssOpenFolders(const QStringList &folders);
    QByteArray getRssSideSplitterState() const;
    void setRssSideSplitterState(const QByteArray &state);
    QByteArray getRssMainSplitterState() const;
    void setRssMainSplitterState(const QByteArray &state);
    QByteArray getSearchTabHeaderState() const;
    void setSearchTabHeaderState(const QByteArray &state);
    bool getRegexAsFilteringPatternForSearchJob() const;
    void setRegexAsFilteringPatternForSearchJob(bool checked);
    QStringList getSearchEngDisabled() const;
    void setSearchEngDisabled(const QStringList &engines);
    bool getStatusFilterState() const;
    bool getCategoryFilterState() const;
    bool getTagFilterState() const;
    bool getTrackerFilterState() const;
    int getTransSelFilter() const;
    void setTransSelFilter(int index);
    QByteArray getTransHeaderState() const;
    void setTransHeaderState(const QByteArray &state);
    bool getRegexAsFilteringPatternForTransferList() const;
    void setRegexAsFilteringPatternForTransferList(bool checked);
    int getToolbarTextPosition() const;
    void setToolbarTextPosition(int position);
    QList<QNetworkCookie> getNetworkCookies() const;
    void setNetworkCookies(const QList<QNetworkCookie> &cookies);

    ITEM(isDHTEnabled, bool, "BitTorrent/Session/DHTEnabled", true);
    ITEM(globalMaxRatio, qreal, "BitTorrent/Session/GlobalMaxRatio", -1, [](qreal r) { return r < 0 ? -1. : r; });
    ITEM(maxActiveDownloads, int, "BitTorrent/Session/MaxActiveDownloads", 3, lowerLimited(-1));
    ITEM(maxConnections, int, "BitTorrent/Session/MaxConnections", 500, lowerLimited(0, -1));

public slots:
    void setStatusFilterState(bool checked);
    void setCategoryFilterState(bool checked);
    void setTagFilterState(bool checked);
    void setTrackerFilterState(bool checked);
};

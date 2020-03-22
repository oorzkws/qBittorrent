/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006-2012  Ishan Arora <ishan@qbittorrent.org>
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

#include "appcontroller.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QTranslator>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/net/portforwarder.h"
#include "base/preferences.h"
#include "base/scanfoldersmodel.h"
#include "base/torrentfileguard.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/password.h"
#include "../webapplication.h"

void AppController::webapiVersionAction()
{
    setResult(static_cast<QString>(API_VERSION));
}

void AppController::versionAction()
{
    setResult(QBT_VERSION);
}

void AppController::buildInfoAction()
{
    const QJsonObject versions = {
        {"qt", QT_VERSION_STR},
        {"libtorrent", Utils::Misc::libtorrentVersionString()},
        {"boost", Utils::Misc::boostVersionString()},
        {"openssl", Utils::Misc::opensslVersionString()},
        {"zlib", Utils::Misc::zlibVersionString()},
        {"bitness", (QT_POINTER_SIZE * 8)}
    };
    setResult(versions);
}

void AppController::shutdownAction()
{
    qDebug() << "Shutdown request from Web UI";

    // Special case handling for shutdown, we
    // need to reply to the Web UI before
    // actually shutting down.
    QTimer::singleShot(100, qApp, &QCoreApplication::quit);
}

void AppController::preferencesAction()
{
    const Preferences *const pref = Preferences::instance();
    QJsonObject data;

    // Downloads
    // When adding a torrent
    data["create_subfolder_enabled"] = pref->isCreateTorrentSubfolderEnabled();
    data["start_paused_enabled"] = pref->isAddTorrentPaused();
    data["auto_delete_mode"] = static_cast<int>(TorrentFileGuard::autoDeleteMode());
    data["preallocate_all"] = pref->isPreallocationEnabled();
    data["incomplete_files_ext"] = pref->isAppendExtensionEnabled();
    // Saving Management
    data["auto_tmm_enabled"] = !pref->isAutoTMMDisabledByDefault();
    data["torrent_changed_tmm_enabled"] = !pref->isDisableAutoTMMWhenCategoryChanged();
    data["save_path_changed_tmm_enabled"] = !pref->isDisableAutoTMMWhenDefaultSavePathChanged();
    data["category_changed_tmm_enabled"] = !pref->isDisableAutoTMMWhenCategorySavePathChanged();
    data["save_path"] = Utils::Fs::toNativePath(pref->defaultSavePath());
    data["temp_path_enabled"] = pref->isTempPathEnabled();
    data["temp_path"] = Utils::Fs::toNativePath(pref->tempPath());
    data["export_dir"] = Utils::Fs::toNativePath(pref->torrentExportDirectory());
    data["export_dir_fin"] = Utils::Fs::toNativePath(pref->finishedTorrentExportDirectory());
    // Automatically add torrents from
    const QVariantHash dirs = pref->getScanDirs();
    QJsonObject nativeDirs;
    for (auto i = dirs.cbegin(); i != dirs.cend(); ++i) {
        if (i.value().type() == QVariant::Int)
            nativeDirs.insert(Utils::Fs::toNativePath(i.key()), i.value().toInt());
        else
            nativeDirs.insert(Utils::Fs::toNativePath(i.key()), Utils::Fs::toNativePath(i.value().toString()));
    }
    data["scan_dirs"] = nativeDirs;
    // Email notification upon download completion
    data["mail_notification_enabled"] = pref->isMailNotificationEnabled();
    data["mail_notification_sender"] = pref->getMailNotificationSender();
    data["mail_notification_email"] = pref->getMailNotificationEmail();
    data["mail_notification_smtp"] = pref->getMailNotificationSMTP();
    data["mail_notification_ssl_enabled"] = pref->getMailNotificationSMTPSSL();
    data["mail_notification_auth_enabled"] = pref->getMailNotificationSMTPAuth();
    data["mail_notification_username"] = pref->getMailNotificationSMTPUsername();
    data["mail_notification_password"] = pref->getMailNotificationSMTPPassword();
    // Run an external program on torrent completion
    data["autorun_enabled"] = pref->isAutoRunEnabled();
    data["autorun_program"] = Utils::Fs::toNativePath(pref->getAutoRunProgram());

    // Connection
    // Listening Port
    data["listen_port"] = pref->port();
    data["upnp"] = pref->isPortForwardingEnabled();
    data["random_port"] = pref->useRandomPort();
    // Connections Limits
    data["max_connec"] = pref->maxConnections();
    data["max_connec_per_torrent"] = pref->maxConnectionsPerTorrent();
    data["max_uploads"] = pref->maxUploads();
    data["max_uploads_per_torrent"] = pref->maxUploadsPerTorrent();

    // Proxy Server
    const Net::ProxyType proxyType = pref->proxyType();
    data["proxy_type"] = static_cast<int>(proxyType);
    data["proxy_ip"] = pref->proxyIP();
    data["proxy_port"] = pref->proxyPort();
    data["proxy_auth_enabled"] = Net::isProxyAuthenticationRequired(proxyType); // deprecated
    data["proxy_username"] = pref->proxyUsername();
    data["proxy_password"] = pref->proxyPassword();

    data["proxy_peer_connections"] = pref->isProxyPeerConnectionsEnabled();
    data["proxy_torrents_only"] = pref->isProxyOnlyForTorrents();

    // IP Filtering
    data["ip_filter_enabled"] = pref->isIPFilteringEnabled();
    data["ip_filter_path"] = Utils::Fs::toNativePath(pref->ipFilterFile());
    data["ip_filter_trackers"] = pref->isTrackerFilteringEnabled();

    // Speed
    // Global Rate Limits
    data["dl_limit"] = pref->globalDownloadSpeedLimit();
    data["up_limit"] = pref->globalUploadSpeedLimit();
    data["alt_dl_limit"] = pref->altGlobalDownloadSpeedLimit();
    data["alt_up_limit"] = pref->altGlobalUploadSpeedLimit();
    data["bittorrent_protocol"] = static_cast<int>(pref->btProtocol());
    data["limit_utp_rate"] = pref->isUTPRateLimited();
    data["limit_tcp_overhead"] = pref->includeOverheadInLimits();
    data["limit_lan_peers"] = !pref->ignoreLimitsOnLAN();
    // Scheduling
    data["scheduler_enabled"] = pref->isBandwidthSchedulerEnabled();
    const QTime start_time = pref->getSchedulerStartTime();
    data["schedule_from_hour"] = start_time.hour();
    data["schedule_from_min"] = start_time.minute();
    const QTime end_time = pref->getSchedulerEndTime();
    data["schedule_to_hour"] = end_time.hour();
    data["schedule_to_min"] = end_time.minute();
    data["scheduler_days"] = pref->getSchedulerDays();

    // Bittorrent
    // Privacy
    data["dht"] = pref->isDHTEnabled();
    data["pex"] = pref->isPeXEnabled();
    data["lsd"] = pref->isLSDEnabled();
    data["encryption"] = pref->encryptionMode();
    data["anonymous_mode"] = pref->isAnonymousModeEnabled();
    // Torrent Queueing
    data["queueing_enabled"] = pref->isQueueingSystemEnabled();
    data["max_active_downloads"] = pref->maxActiveDownloads();
    data["max_active_torrents"] = pref->maxActiveTorrents();
    data["max_active_uploads"] = pref->maxActiveUploads();
    data["dont_count_slow_torrents"] = pref->ignoreSlowTorrentsForQueueing();
    data["slow_torrent_dl_rate_threshold"] = pref->downloadRateForSlowTorrents();
    data["slow_torrent_ul_rate_threshold"] = pref->uploadRateForSlowTorrents();
    data["slow_torrent_inactive_timer"] = pref->slowTorrentsInactivityTimer();
    // Share Ratio Limiting
    data["max_ratio_enabled"] = (pref->globalMaxRatio() >= 0.);
    data["max_ratio"] = pref->globalMaxRatio();
    data["max_seeding_time_enabled"] = (pref->globalMaxSeedingMinutes() >= 0.);
    data["max_seeding_time"] = pref->globalMaxSeedingMinutes();
    data["max_ratio_act"] = pref->maxRatioAction();
    // Add trackers
    data["add_trackers_enabled"] = pref->isAddTrackersEnabled();
    data["add_trackers"] = pref->additionalTrackers();

    // Web UI
    // Language
    data["locale"] = pref->getLocale();
    // HTTP Server
    data["web_ui_domain_list"] = pref->getServerDomains();
    data["web_ui_address"] = pref->getWebUiAddress();
    data["web_ui_port"] = pref->getWebUiPort();
    data["web_ui_upnp"] = pref->useUPnPForWebUIPort();
    data["use_https"] = pref->isWebUiHttpsEnabled();
    data["web_ui_https_cert_path"] = pref->getWebUIHttpsCertificatePath();
    data["web_ui_https_key_path"] = pref->getWebUIHttpsKeyPath();
    // Authentication
    data["web_ui_username"] = pref->getWebUiUsername();
    data["bypass_local_auth"] = !pref->isWebUiLocalAuthEnabled();
    data["bypass_auth_subnet_whitelist_enabled"] = pref->isWebUiAuthSubnetWhitelistEnabled();
    QStringList authSubnetWhitelistStringList;
    for (const Utils::Net::Subnet &subnet : asConst(pref->getWebUiAuthSubnetWhitelist()))
        authSubnetWhitelistStringList << Utils::Net::subnetToString(subnet);
    data["bypass_auth_subnet_whitelist"] = authSubnetWhitelistStringList.join("\n");
    data["web_ui_max_auth_fail_count"] = pref->getWebUIMaxAuthFailCount();
    data["web_ui_ban_duration"] = static_cast<int>(pref->getWebUIBanDuration().count());
    data["web_ui_session_timeout"] = pref->getWebUISessionTimeout();
    // Use alternative Web UI
    data["alternative_webui_enabled"] = pref->isAltWebUiEnabled();
    data["alternative_webui_path"] = pref->getWebUiRootFolder();
    // Security
    data["web_ui_clickjacking_protection_enabled"] = pref->isWebUiClickjackingProtectionEnabled();
    data["web_ui_csrf_protection_enabled"] = pref->isWebUiCSRFProtectionEnabled();
    data["web_ui_secure_cookie_enabled"] = pref->isWebUiSecureCookieEnabled();
    data["web_ui_host_header_validation_enabled"] = pref->isWebUIHostHeaderValidationEnabled();
    // Update my dynamic domain name
    data["dyndns_enabled"] = pref->isDynDNSEnabled();
    data["dyndns_service"] = pref->getDynDNSService();
    data["dyndns_username"] = pref->getDynDNSUsername();
    data["dyndns_password"] = pref->getDynDNSPassword();
    data["dyndns_domain"] = pref->getDynDomainName();

    // RSS settings
    data["rss_refresh_interval"] = pref->getRSSRefreshInterval();
    data["rss_max_articles_per_feed"] = pref->getRSSMaxArticlesPerFeed();
    data["rss_processing_enabled"] = pref->isRSSProcessingEnabled();
    data["rss_auto_downloading_enabled"] = pref->isRSSAutoDownloadingEnabled();

    // Advanced settings
    // qBitorrent preferences
    // Current network interface
    data["current_network_interface"] = pref->networkInterface();
    // Current network interface address
    data["current_interface_address"] = BitTorrent::Session::instance()->networkInterfaceAddress();
    // Save resume data interval
    data["save_resume_data_interval"] = static_cast<double>(pref->saveResumeDataInterval());
    // Recheck completed torrents
    data["recheck_completed_torrents"] = pref->recheckTorrentsOnCompletion();
    // Resolve peer countries
    data["resolve_peer_countries"] = pref->resolvePeerCountries();

    // libtorrent preferences
    // Async IO threads
    data["async_io_threads"] = pref->asyncIOThreadsCount();
    // File pool size
    data["file_pool_size"] = pref->filePoolSize();
    // Checking memory usage
    data["checking_memory_use"] = pref->checkingMemUsage();
    // Disk write cache
    data["disk_cache"] = pref->diskCacheSize();
    data["disk_cache_ttl"] = pref->diskCacheTTL();
    // Enable OS cache
    data["enable_os_cache"] = pref->useOSCache();
    // Coalesce reads & writes
    data["enable_coalesce_read_write"] = pref->isCoalesceReadWriteEnabled();
    // Piece Extent Affinity
    data["enable_piece_extent_affinity"] = pref->usePieceExtentAffinity();
    // Suggest mode
    data["enable_upload_suggestions"] = pref->isSuggestModeEnabled();
    // Send buffer watermark
    data["send_buffer_watermark"] = pref->sendBufferWatermark();
    data["send_buffer_low_watermark"] = pref->sendBufferLowWatermark();
    data["send_buffer_watermark_factor"] = pref->sendBufferWatermarkFactor();
    // Socket listen backlog size
    data["socket_backlog_size"] = pref->socketBacklogSize();
    // Outgoing ports
    data["outgoing_ports_min"] = pref->minOutgoingPort();
    data["outgoing_ports_max"] = pref->maxOutgoingPort();
    // uTP-TCP mixed mode
    data["utp_tcp_mixed_mode"] = static_cast<int>(pref->utpMixedMode());
    // Multiple connections per IP
    data["enable_multi_connections_from_same_ip"] = pref->isMultiConnectionsPerIPEnabled();
    // Embedded tracker
    data["enable_embedded_tracker"] = pref->isTrackerEnabled();
    data["embedded_tracker_port"] = pref->getTrackerPort();
    // Choking algorithm
    data["upload_slots_behavior"] = static_cast<int>(pref->chokingAlgorithm());
    // Seed choking algorithm
    data["upload_choking_algorithm"] = static_cast<int>(pref->seedChokingAlgorithm());
    // Super seeding
    data["enable_super_seeding"] = pref->isSuperSeedingEnabled();
    // Announce
    data["announce_to_all_trackers"] = pref->announceToAllTrackers();
    data["announce_to_all_tiers"] = pref->announceToAllTiers();
    data["announce_ip"] = pref->announceIP();
    // Stop tracker timeout
    data["stop_tracker_timeout"] = pref->stopTrackerTimeout();

    setResult(data);
}

void AppController::setPreferencesAction()
{
    requireParams({"json"});

    Preferences *const pref = Preferences::instance();
    const QVariantHash m = QJsonDocument::fromJson(params()["json"].toUtf8()).toVariant().toHash();

    QVariantHash::ConstIterator it;
    const auto hasKey = [&it, &m](const char *key) -> bool
    {
        it = m.find(QLatin1String(key));
        return (it != m.constEnd());
    };

    // Downloads
    // When adding a torrent
    if (hasKey("create_subfolder_enabled"))
        pref->isCreateTorrentSubfolderEnabled.set(it.value().toBool());
    if (hasKey("start_paused_enabled"))
        pref->isAddTorrentPaused.set(it.value().toBool());
    if (hasKey("auto_delete_mode"))
        TorrentFileGuard::setAutoDeleteMode(static_cast<TorrentFileGuard::AutoDeleteMode>(it.value().toInt()));

    if (hasKey("preallocate_all"))
        pref->isPreallocationEnabled.set(it.value().toBool());
    if (hasKey("incomplete_files_ext"))
        pref->isAppendExtensionEnabled.set(it.value().toBool());

    // Saving Management
    if (hasKey("auto_tmm_enabled"))
        pref->isAutoTMMDisabledByDefault.set(!it.value().toBool());
    if (hasKey("torrent_changed_tmm_enabled"))
        pref->isDisableAutoTMMWhenCategoryChanged.set(!it.value().toBool());
    if (hasKey("save_path_changed_tmm_enabled"))
        pref->isDisableAutoTMMWhenDefaultSavePathChanged.set(!it.value().toBool());
    if (hasKey("category_changed_tmm_enabled"))
        pref->isDisableAutoTMMWhenCategorySavePathChanged.set(!it.value().toBool());
    if (hasKey("save_path"))
        pref->defaultSavePath.set(it.value().toString());
    if (hasKey("temp_path_enabled"))
        pref->isTempPathEnabled.set(it.value().toBool());
    if (hasKey("temp_path"))
        pref->tempPath.set(it.value().toString());
    if (hasKey("export_dir"))
        pref->torrentExportDirectory.set(it.value().toString());
    if (hasKey("export_dir_fin"))
        pref->finishedTorrentExportDirectory.set(it.value().toString());
    // Automatically add torrents from
    if (hasKey("scan_dirs")) {
        const QVariantHash nativeDirs = it.value().toHash();
        QVariantHash oldScanDirs = pref->getScanDirs();
        QVariantHash scanDirs;
        ScanFoldersModel *model = ScanFoldersModel::instance();
        for (auto i = nativeDirs.cbegin(); i != nativeDirs.cend(); ++i) {
            QString folder = Utils::Fs::toUniformPath(i.key());
            int downloadType;
            QString downloadPath;
            ScanFoldersModel::PathStatus ec;
            if (i.value().type() == QVariant::String) {
                downloadType = ScanFoldersModel::CUSTOM_LOCATION;
                downloadPath = Utils::Fs::toUniformPath(i.value().toString());
            }
            else {
                downloadType = i.value().toInt();
                downloadPath = (downloadType == ScanFoldersModel::DEFAULT_LOCATION) ? "Default folder" : "Watch folder";
            }

            if (!oldScanDirs.contains(folder))
                ec = model->addPath(folder, static_cast<ScanFoldersModel::PathType>(downloadType), downloadPath);
            else
                ec = model->updatePath(folder, static_cast<ScanFoldersModel::PathType>(downloadType), downloadPath);

            if (ec == ScanFoldersModel::Ok) {
                scanDirs.insert(folder, (downloadType == ScanFoldersModel::CUSTOM_LOCATION) ? QVariant(downloadPath) : QVariant(downloadType));
                qDebug("New watched folder: %s to %s", qUtf8Printable(folder), qUtf8Printable(downloadPath));
            }
            else {
                qDebug("Watched folder %s failed with error %d", qUtf8Printable(folder), ec);
            }
        }

        // Update deleted folders
        for (auto i = oldScanDirs.cbegin(); i != oldScanDirs.cend(); ++i) {
            const QString &folder = i.key();
            if (!scanDirs.contains(folder)) {
                model->removePath(folder);
                qDebug("Removed watched folder %s", qUtf8Printable(folder));
            }
        }
        pref->setScanDirs(scanDirs);
    }
    // Email notification upon download completion
    if (hasKey("mail_notification_enabled"))
        pref->setMailNotificationEnabled(it.value().toBool());
    if (hasKey("mail_notification_sender"))
        pref->setMailNotificationSender(it.value().toString());
    if (hasKey("mail_notification_email"))
        pref->setMailNotificationEmail(it.value().toString());
    if (hasKey("mail_notification_smtp"))
        pref->setMailNotificationSMTP(it.value().toString());
    if (hasKey("mail_notification_ssl_enabled"))
        pref->setMailNotificationSMTPSSL(it.value().toBool());
    if (hasKey("mail_notification_auth_enabled"))
        pref->setMailNotificationSMTPAuth(it.value().toBool());
    if (hasKey("mail_notification_username"))
        pref->setMailNotificationSMTPUsername(it.value().toString());
    if (hasKey("mail_notification_password"))
        pref->setMailNotificationSMTPPassword(it.value().toString());
    // Run an external program on torrent completion
    if (hasKey("autorun_enabled"))
        pref->setAutoRunEnabled(it.value().toBool());
    if (hasKey("autorun_program"))
        pref->setAutoRunProgram(it.value().toString());

    // Connection
    // Listening Port
    if (hasKey("listen_port"))
        pref->port.set(it.value().toInt());
    if (hasKey("upnp"))
        pref->setPortForwardingEnabled(it.value().toBool());
    if (hasKey("random_port"))
        pref->useRandomPort.set(it.value().toBool());
    // Connections Limits
    if (hasKey("max_connec"))
        pref->maxConnections.set(it.value().toInt());
    if (hasKey("max_connec_per_torrent"))
        pref->maxConnectionsPerTorrent.set(it.value().toInt());
    if (hasKey("max_uploads"))
        pref->maxUploads.set(it.value().toInt());
    if (hasKey("max_uploads_per_torrent"))
        pref->maxUploadsPerTorrent.set(it.value().toInt());

    // Proxy Server
    if (hasKey("proxy_type"))
        pref->setProxyType(static_cast<Net::ProxyType>(it.value().toInt()));
    if (hasKey("proxy_ip"))
        pref->setProxyIP(it.value().toString());
    if (hasKey("proxy_port"))
        pref->setProxyPort(it.value().toInt());
    if (hasKey("proxy_username"))
        pref->setProxyUsername(it.value().toString());
    if (hasKey("proxy_password"))
        pref->setProxyPassword(it.value().toString());
    if (hasKey("proxy_torrents_only"))
        pref->setProxyOnlyForTorrents(it.value().toBool());
    if (hasKey("proxy_peer_connections"))
        pref->isProxyPeerConnectionsEnabled.set(it.value().toBool());

    // IP Filtering
    if (hasKey("ip_filter_enabled"))
        pref->isIPFilteringEnabled.set(it.value().toBool());
    if (hasKey("ip_filter_path"))
        pref->ipFilterFile.set(it.value().toString());
    if (hasKey("ip_filter_trackers"))
        pref->isTrackerFilteringEnabled.set(it.value().toBool());

    // Speed
    // Global Rate Limits
    if (hasKey("dl_limit"))
        pref->globalDownloadSpeedLimit.set(it.value().toInt());
    if (hasKey("up_limit"))
        pref->globalUploadSpeedLimit.set(it.value().toInt());
    if (hasKey("alt_dl_limit"))
        pref->altGlobalDownloadSpeedLimit.set(it.value().toInt());
    if (hasKey("alt_up_limit"))
       pref->altGlobalUploadSpeedLimit.set(it.value().toInt());
    if (hasKey("bittorrent_protocol"))
        pref->btProtocol.set(static_cast<BitTorrent::BTProtocol>(it.value().toInt()));
    if (hasKey("limit_utp_rate"))
        pref->isUTPRateLimited.set(it.value().toBool());
    if (hasKey("limit_tcp_overhead"))
        pref->includeOverheadInLimits.set(it.value().toBool());
    if (hasKey("limit_lan_peers"))
        pref->ignoreLimitsOnLAN.set(!it.value().toBool());
    // Scheduling
    if (hasKey("scheduler_enabled"))
        pref->isBandwidthSchedulerEnabled.set(it.value().toBool());
    if (m.contains("schedule_from_hour") && m.contains("schedule_from_min"))
        pref->setSchedulerStartTime(QTime(m["schedule_from_hour"].toInt(), m["schedule_from_min"].toInt()));
    if (m.contains("schedule_to_hour") && m.contains("schedule_to_min"))
        pref->setSchedulerEndTime(QTime(m["schedule_to_hour"].toInt(), m["schedule_to_min"].toInt()));
    if (hasKey("scheduler_days"))
        pref->setSchedulerDays(SchedulerDays(it.value().toInt()));

    // Bittorrent
    // Privacy
    if (hasKey("dht"))
        pref->isDHTEnabled.set(it.value().toBool());
    if (hasKey("pex"))
        pref->isPeXEnabled.set(it.value().toBool());
    if (hasKey("lsd"))
        pref->isLSDEnabled.set(it.value().toBool());
    if (hasKey("encryption"))
        pref->encryptionMode.set(it.value().toInt());
    if (hasKey("anonymous_mode"))
        pref->isAnonymousModeEnabled.set(it.value().toBool());
    // Torrent Queueing
    if (hasKey("queueing_enabled"))
        pref->isQueueingSystemEnabled.set(it.value().toBool());
    if (hasKey("max_active_downloads"))
        pref->maxActiveDownloads.set(it.value().toInt());
    if (hasKey("max_active_torrents"))
        pref->maxActiveTorrents.set(it.value().toInt());
    if (hasKey("max_active_uploads"))
        pref->maxActiveUploads.set(it.value().toInt());
    if (hasKey("dont_count_slow_torrents"))
        pref->ignoreSlowTorrentsForQueueing.set(it.value().toBool());
    if (hasKey("slow_torrent_dl_rate_threshold"))
        pref->downloadRateForSlowTorrents.set(it.value().toInt());
    if (hasKey("slow_torrent_ul_rate_threshold"))
        pref->uploadRateForSlowTorrents.set(it.value().toInt());
    if (hasKey("slow_torrent_inactive_timer"))
        pref->slowTorrentsInactivityTimer.set(it.value().toInt());
    // Share Ratio Limiting
    if (hasKey("max_ratio_enabled")) {
        if (it.value().toBool())
            pref->globalMaxRatio.set(m["max_ratio"].toReal());
        else
            pref->globalMaxRatio.set(-1);
    }
    if (hasKey("max_seeding_time_enabled")) {
        if (it.value().toBool())
            pref->globalMaxSeedingMinutes.set(m["max_seeding_time"].toInt());
        else
            pref->globalMaxSeedingMinutes.set(-1);
    }
    if (hasKey("max_ratio_act"))
        pref->maxRatioAction.set(static_cast<MaxRatioAction>(it.value().toInt()));
    // Add trackers
    pref->isAddTrackersEnabled.set(m["add_trackers_enabled"].toBool());
    pref->additionalTrackers.set(m["add_trackers"].toString());

    // Web UI
    // Language
    if (hasKey("locale")) {
        QString locale = it.value().toString();
        if (pref->getLocale() != locale) {
            auto *translator = new QTranslator;
            if (translator->load(QLatin1String(":/lang/qbittorrent_") + locale)) {
                qDebug("%s locale recognized, using translation.", qUtf8Printable(locale));
            }
            else {
                qDebug("%s locale unrecognized, using default (en).", qUtf8Printable(locale));
            }
            qApp->installTranslator(translator);

            pref->setLocale(locale);
        }
    }
    // HTTP Server
    if (hasKey("web_ui_domain_list"))
        pref->setServerDomains(it.value().toString());
    if (hasKey("web_ui_address"))
        pref->setWebUiAddress(it.value().toString());
    if (hasKey("web_ui_port"))
        pref->setWebUiPort(it.value().toUInt());
    if (hasKey("web_ui_upnp"))
        pref->setUPnPForWebUIPort(it.value().toBool());
    if (hasKey("use_https"))
        pref->setWebUiHttpsEnabled(it.value().toBool());
    if (hasKey("web_ui_https_cert_path"))
        pref->setWebUIHttpsCertificatePath(it.value().toString());
    if (hasKey("web_ui_https_key_path"))
        pref->setWebUIHttpsKeyPath(it.value().toString());
    // Authentication
    if (hasKey("web_ui_username"))
        pref->setWebUiUsername(it.value().toString());
    if (hasKey("web_ui_password"))
        pref->setWebUIPassword(Utils::Password::PBKDF2::generate(it.value().toByteArray()));
    if (hasKey("bypass_local_auth"))
        pref->setWebUiLocalAuthEnabled(!it.value().toBool());
    if (hasKey("bypass_auth_subnet_whitelist_enabled"))
        pref->setWebUiAuthSubnetWhitelistEnabled(it.value().toBool());
    if (hasKey("bypass_auth_subnet_whitelist")) {
        // recognize new lines and commas as delimiters
        pref->setWebUiAuthSubnetWhitelist(it.value().toString().split(QRegularExpression("\n|,"), QString::SkipEmptyParts));
    }
    if (hasKey("web_ui_max_auth_fail_count"))
        pref->setWebUIMaxAuthFailCount(it.value().toInt());
    if (hasKey("web_ui_ban_duration"))
        pref->setWebUIBanDuration(std::chrono::seconds {it.value().toInt()});
    if (hasKey("web_ui_session_timeout"))
        pref->setWebUISessionTimeout(it.value().toInt());
    // Use alternative Web UI
    if (hasKey("alternative_webui_enabled"))
        pref->setAltWebUiEnabled(it.value().toBool());
    if (hasKey("alternative_webui_path"))
        pref->setWebUiRootFolder(it.value().toString());
    // Security
    if (hasKey("web_ui_clickjacking_protection_enabled"))
        pref->setWebUiClickjackingProtectionEnabled(it.value().toBool());
    if (hasKey("web_ui_csrf_protection_enabled"))
        pref->setWebUiCSRFProtectionEnabled(it.value().toBool());
    if (hasKey("web_ui_secure_cookie_enabled"))
        pref->setWebUiSecureCookieEnabled(it.value().toBool());
    if (hasKey("web_ui_host_header_validation_enabled"))
        pref->setWebUIHostHeaderValidationEnabled(it.value().toBool());
    // Update my dynamic domain name
    if (hasKey("dyndns_enabled"))
        pref->setDynDNSEnabled(it.value().toBool());
    if (hasKey("dyndns_service"))
        pref->setDynDNSService(it.value().toInt());
    if (hasKey("dyndns_username"))
        pref->setDynDNSUsername(it.value().toString());
    if (hasKey("dyndns_password"))
        pref->setDynDNSPassword(it.value().toString());
    if (hasKey("dyndns_domain"))
        pref->setDynDomainName(it.value().toString());

    if (hasKey("rss_refresh_interval"))
        pref->setRSSRefreshInterval(it.value().toInt());
    if (hasKey("rss_max_articles_per_feed"))
        pref->setRSSMaxArticlesPerFeed(it.value().toInt());
    if (hasKey("rss_processing_enabled"))
        pref->setRSSProcessingEnabled(it.value().toBool());
    if (hasKey("rss_auto_downloading_enabled"))
        pref->setRSSAutoDownloadingEnabled(it.value().toBool());

    // Advanced settings
    // qBittorrent preferences
    // Current network interface
    if (hasKey("current_network_interface")) {
        const QString ifaceValue {it.value().toString()};

        const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        const auto ifacesIter = std::find_if(ifaces.cbegin(), ifaces.cend(), [&ifaceValue](const QNetworkInterface &iface)
        {
            return (!iface.addressEntries().isEmpty()) && (iface.name() == ifaceValue);
        });
        const QString ifaceName = (ifacesIter != ifaces.cend()) ? ifacesIter->humanReadableName() : QString {};

        pref->networkInterface.set(ifaceValue);
        pref->networkInterfaceName.set(ifaceName);
    }
    // Current network interface address
    if (hasKey("current_interface_address")) {
        const QHostAddress ifaceAddress {it.value().toString().trimmed()};
        pref->networkInterfaceAddress.set(ifaceAddress.isNull() ? QString {} : ifaceAddress.toString());
    }
    // Save resume data interval
    if (hasKey("save_resume_data_interval"))
        pref->saveResumeDataInterval.set(it.value().toInt());
    // Recheck completed torrents
    if (hasKey("recheck_completed_torrents"))
        pref->recheckTorrentsOnCompletion(it.value().toBool());
    // Resolve peer countries
    if (hasKey("resolve_peer_countries"))
        pref->resolvePeerCountries(it.value().toBool());

    // libtorrent preferences
    // Async IO threads
    if (hasKey("async_io_threads"))
        pref->asyncIOThreadsCount.set(it.value().toInt());
    // File pool size
    if (hasKey("file_pool_size"))
        pref->filePoolSize.set(it.value().toInt());
    // Checking Memory Usage
    if (hasKey("checking_memory_use"))
        pref->checkingMemUsage.set(it.value().toInt());
    // Disk write cache
    if (hasKey("disk_cache"))
        pref->diskCacheSize.set(it.value().toInt());
    if (hasKey("disk_cache_ttl"))
        pref->diskCacheTTL.set(it.value().toInt());
    // Enable OS cache
    if (hasKey("enable_os_cache"))
        pref->useOSCache.set(it.value().toBool());
    // Coalesce reads & writes
    if (hasKey("enable_coalesce_read_write"))
        pref->isCoalesceReadWriteEnabled.set(it.value().toBool());
    // Piece extent affinity
    if (hasKey("enable_piece_extent_affinity"))
        pref->usePieceExtentAffinity.set(it.value().toBool());
    // Suggest mode
    if (hasKey("enable_upload_suggestions"))
        pref->isSuggestModeEnabled.set(it.value().toBool());
    // Send buffer watermark
    if (hasKey("send_buffer_watermark"))
        pref->sendBufferWatermark.set(it.value().toInt());
    if (hasKey("send_buffer_low_watermark"))
        pref->sendBufferLowWatermark.set(it.value().toInt());
    if (hasKey("send_buffer_watermark_factor"))
        pref->sendBufferWatermarkFactor.set(it.value().toInt());
    // Socket listen backlog size
    if (hasKey("socket_backlog_size"))
        pref->socketBacklogSize.set(it.value().toInt());
    // Outgoing ports
    if (hasKey("outgoing_ports_min"))
        pref->minOutgoingPort.set(it.value().toInt());
    if (hasKey("outgoing_ports_max"))
        pref->maxOutgoingPort.set(it.value().toInt());
    // uTP-TCP mixed mode
    if (hasKey("utp_tcp_mixed_mode"))
        pref->utpMixedMode.set(static_cast<BitTorrent::MixedModeAlgorithm>(it.value().toInt()));
    // Multiple connections per IP
    if (hasKey("enable_multi_connections_from_same_ip"))
        pref->isMultiConnectionsPerIPEnabled.set(it.value().toBool());
    // Embedded tracker
    if (hasKey("embedded_tracker_port"))
        pref->setTrackerPort(it.value().toInt());
    if (hasKey("enable_embedded_tracker"))
        pref->isTrackerEnabled.set(it.value().toBool());
    // Choking algorithm
    if (hasKey("upload_slots_behavior"))
        pref->chokingAlgorithm.set(static_cast<BitTorrent::ChokingAlgorithm>(it.value().toInt()));
    // Seed choking algorithm
    if (hasKey("upload_choking_algorithm"))
        pref->seedChokingAlgorithm.set(static_cast<BitTorrent::SeedChokingAlgorithm>(it.value().toInt()));
    // Super seeding
    if (hasKey("enable_super_seeding"))
        pref->isSuperSeedingEnabled.set(it.value().toBool());
    // Announce
    if (hasKey("announce_to_all_trackers"))
        pref->announceToAllTrackers.set(it.value().toBool());
    if (hasKey("announce_to_all_tiers"))
        pref->announceToAllTiers.set(it.value().toBool());
    if (hasKey("announce_ip")) {
        const QHostAddress announceAddr {it.value().toString().trimmed()};
        pref->announceIP.set(announceAddr.isNull() ? QString {} : announceAddr.toString());
    }
    // Stop tracker timeout
    if (hasKey("stop_tracker_timeout"))
        pref->stopTrackerTimeout.set(it.value().toInt());

    // Save preferences
    pref->notifyChanged();
}

void AppController::defaultSavePathAction()
{
    setResult(BitTorrent::Session::instance()->defaultSavePath());
}

void AppController::networkInterfaceListAction()
{
    QJsonArray ifaceList;
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces())) {
        if (!iface.addressEntries().isEmpty()) {
            ifaceList.append(QJsonObject {
                {"name", iface.humanReadableName()},
                {"value", iface.name()}
            });
        }
    }

    setResult(ifaceList);
}

void AppController::networkInterfaceAddressListAction()
{
    requireParams({"iface"});

    const QString ifaceName = params().value("iface");
    QJsonArray addressList;

    const auto appendAddress = [&addressList](const QHostAddress &addr)
    {
        if (addr.protocol() == QAbstractSocket::IPv6Protocol)
            addressList.append(Utils::Net::canonicalIPv6Addr(addr).toString());
        else
            addressList.append(addr.toString());
    };

    if (ifaceName.isEmpty()) {
        for (const QHostAddress &addr : asConst(QNetworkInterface::allAddresses()))
            appendAddress(addr);
    }
    else {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
        for (const QNetworkAddressEntry &entry : asConst(iface.addressEntries()))
            appendAddress(entry.ip());
    }

    setResult(addressList);
}

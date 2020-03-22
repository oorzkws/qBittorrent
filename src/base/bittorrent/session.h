/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef BITTORRENT_SESSION_H
#define BITTORRENT_SESSION_H

#include <vector>

#include <libtorrent/fwd.hpp>

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>

#include "base/basedefs.h"
#include "addtorrentparams.h"
#include "bittorrentdefs.h"
#include "cachestatus.h"
#include "sessionstatus.h"
#include "torrentinfo.h"

class QFile;
class QNetworkConfiguration;
class QNetworkConfigurationManager;
class QString;
class QStringList;
class QThread;
class QTimer;
class QUrl;

class BandwidthScheduler;
class FilterParserThread;
class ResumeDataSavingManager;
class Statistics;

// These values should remain unchanged when adding new items
// so as not to break the existing user settings.
enum MaxRatioAction
{
    Pause = 0,
    Remove = 1,
    DeleteFiles = 3,
    EnableSuperSeeding = 2
};

enum DeleteOption
{
    Torrent,
    TorrentAndFiles
};

enum TorrentExportFolder
{
    Regular,
    Finished
};

namespace Net
{
    struct DownloadResult;
}

namespace BitTorrent
{
    class InfoHash;
    class MagnetUri;
    class TorrentHandle;
    class Tracker;
    class TrackerEntry;
    struct CreateTorrentParams;

    enum class MoveStorageMode;

    struct SessionMetricIndices
    {
        struct
        {
            int hasIncomingConnections = 0;
            int sentPayloadBytes = 0;
            int recvPayloadBytes = 0;
            int sentBytes = 0;
            int recvBytes = 0;
            int sentIPOverheadBytes = 0;
            int recvIPOverheadBytes = 0;
            int sentTrackerBytes = 0;
            int recvTrackerBytes = 0;
            int recvRedundantBytes = 0;
            int recvFailedBytes = 0;
        } net;

        struct
        {
            int numPeersConnected = 0;
            int numPeersUpDisk = 0;
            int numPeersDownDisk = 0;
        } peer;

        struct
        {
            int dhtBytesIn = 0;
            int dhtBytesOut = 0;
            int dhtNodes = 0;
        } dht;

        struct
        {
            int diskBlocksInUse = 0;
            int numBlocksRead = 0;
            int numBlocksCacheHits = 0;
            int writeJobs = 0;
            int readJobs = 0;
            int hashJobs = 0;
            int queuedDiskJobs = 0;
            int diskJobTime = 0;
        } disk;
    };

    class Session : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Session)

    public:
        static void initInstance();
        static void freeInstance();
        static Session *instance();

        QString defaultSavePath() const;
        void setDefaultSavePath(QString path);
        QString tempPath() const;
        void setTempPath(QString path);
        bool isTempPathEnabled() const;
        void setTempPathEnabled(bool enabled);
        QString torrentTempPath(const TorrentInfo &torrentInfo) const;

        static bool isValidCategoryName(const QString &name);
        // returns category itself and all top level categories
        static QStringList expandCategory(const QString &category);

        QStringMap categories() const;
        QString categorySavePath(const QString &categoryName) const;
        bool addCategory(const QString &name, const QString &savePath = "");
        bool editCategory(const QString &name, const QString &savePath);
        bool removeCategory(const QString &name);
        bool isSubcategoriesEnabled() const;
        void setSubcategoriesEnabled(bool value);

        static bool isValidTag(const QString &tag);
        QSet<QString> tags() const;
        bool hasTag(const QString &tag) const;
        bool addTag(const QString &tag);
        bool removeTag(const QString &tag);

        int downloadSpeedLimit() const;
        void setDownloadSpeedLimit(int limit);
        int uploadSpeedLimit() const;
        void setUploadSpeedLimit(int limit);
        bool isAltGlobalSpeedLimitEnabled() const;
        void setAltGlobalSpeedLimitEnabled(bool enabled);

        void startUpTorrents();
        TorrentHandle *findTorrent(const InfoHash &hash) const;
        QHash<InfoHash, TorrentHandle *> torrents() const;
        bool hasActiveTorrents() const;
        bool hasUnfinishedTorrents() const;
        bool hasRunningSeed() const;
        const SessionStatus &status() const;
        const CacheStatus &cacheStatus() const;
        quint64 getAlltimeDL() const;
        quint64 getAlltimeUL() const;
        bool isListening() const;

        QStringList bannedIPs() const;
        void setBannedIPs(const QStringList &newList);
        void banIP(const QString &ip);

        bool isKnownTorrent(const InfoHash &hash) const;
        bool addTorrent(const QString &source, const AddTorrentParams &params = AddTorrentParams());
        bool addTorrent(const TorrentInfo &torrentInfo, const AddTorrentParams &params = AddTorrentParams());
        bool deleteTorrent(const InfoHash &hash, DeleteOption deleteOption = Torrent);
        bool loadMetadata(const MagnetUri &magnetUri);
        bool cancelLoadMetadata(const InfoHash &hash);

        void recursiveTorrentDownload(const InfoHash &hash);
        void increaseTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void decreaseTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void topTorrentsQueuePos(const QVector<InfoHash> &hashes);
        void bottomTorrentsQueuePos(const QVector<InfoHash> &hashes);

        // TorrentHandle interface
        void handleTorrentSaveResumeDataRequested(const TorrentHandle *torrent);
        void handleTorrentShareLimitChanged(TorrentHandle *const torrent);
        void handleTorrentNameChanged(TorrentHandle *const torrent);
        void handleTorrentSavePathChanged(TorrentHandle *const torrent);
        void handleTorrentCategoryChanged(TorrentHandle *const torrent, const QString &oldCategory);
        void handleTorrentTagAdded(TorrentHandle *const torrent, const QString &tag);
        void handleTorrentTagRemoved(TorrentHandle *const torrent, const QString &tag);
        void handleTorrentSavingModeChanged(TorrentHandle *const torrent);
        void handleTorrentMetadataReceived(TorrentHandle *const torrent);
        void handleTorrentPaused(TorrentHandle *const torrent);
        void handleTorrentResumed(TorrentHandle *const torrent);
        void handleTorrentChecked(TorrentHandle *const torrent);
        void handleTorrentFinished(TorrentHandle *const torrent);
        void handleTorrentTrackersAdded(TorrentHandle *const torrent, const QVector<TrackerEntry> &newTrackers);
        void handleTorrentTrackersRemoved(TorrentHandle *const torrent, const QVector<TrackerEntry> &deletedTrackers);
        void handleTorrentTrackersChanged(TorrentHandle *const torrent);
        void handleTorrentUrlSeedsAdded(TorrentHandle *const torrent, const QVector<QUrl> &newUrlSeeds);
        void handleTorrentUrlSeedsRemoved(TorrentHandle *const torrent, const QVector<QUrl> &urlSeeds);
        void handleTorrentResumeDataReady(TorrentHandle *const torrent, const lt::entry &data);
        void handleTorrentResumeDataFailed(TorrentHandle *const torrent);
        void handleTorrentTrackerReply(TorrentHandle *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerWarning(TorrentHandle *const torrent, const QString &trackerUrl);
        void handleTorrentTrackerError(TorrentHandle *const torrent, const QString &trackerUrl);

        bool addMoveTorrentStorageJob(TorrentHandle *torrent, const QString &newPath, MoveStorageMode mode);

    signals:
        void addTorrentFailed(const QString &error);
        void allTorrentsFinished();
        void categoryAdded(const QString &categoryName);
        void categoryRemoved(const QString &categoryName);
        void downloadFromUrlFailed(const QString &url, const QString &reason);
        void downloadFromUrlFinished(const QString &url);
        void fullDiskError(BitTorrent::TorrentHandle *const torrent, const QString &msg);
        void IPFilterParsed(bool error, int ruleCount);
        void metadataLoaded(const BitTorrent::TorrentInfo &info);
        void recursiveTorrentDownloadPossible(BitTorrent::TorrentHandle *const torrent);
        void speedLimitModeChanged(bool alternative);
        void statsUpdated();
        void subcategoriesSupportChanged();
        void tagAdded(const QString &tag);
        void tagRemoved(const QString &tag);
        void torrentAboutToBeRemoved(BitTorrent::TorrentHandle *const torrent);
        void torrentAdded(BitTorrent::TorrentHandle *const torrent);
        void torrentCategoryChanged(BitTorrent::TorrentHandle *const torrent, const QString &oldCategory);
        void torrentFinished(BitTorrent::TorrentHandle *const torrent);
        void torrentFinishedChecking(BitTorrent::TorrentHandle *const torrent);
        void torrentMetadataLoaded(BitTorrent::TorrentHandle *const torrent);
        void torrentNew(BitTorrent::TorrentHandle *const torrent);
        void torrentPaused(BitTorrent::TorrentHandle *const torrent);
        void torrentResumed(BitTorrent::TorrentHandle *const torrent);
        void torrentSavePathChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentSavingModeChanged(BitTorrent::TorrentHandle *const torrent);
        void torrentsUpdated(const QVector<BitTorrent::TorrentHandle *> &torrents);
        void torrentTagAdded(TorrentHandle *const torrent, const QString &tag);
        void torrentTagRemoved(TorrentHandle *const torrent, const QString &tag);
        void trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerlessStateChanged(BitTorrent::TorrentHandle *const torrent, bool trackerless);
        void trackersAdded(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
        void trackersChanged(BitTorrent::TorrentHandle *const torrent);
        void trackersRemoved(BitTorrent::TorrentHandle *const torrent, const QVector<BitTorrent::TrackerEntry> &trackers);
        void trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker);
        void trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker);

    private slots:
        void configureDeferred();
        void readAlerts();
        void refresh();
        void processShareLimits();
        void generateResumeData(bool final = false);
        void handleIPFilterParsed(int ruleCount);
        void handleIPFilterError();
        void handleDownloadFinished(const Net::DownloadResult &result);

        // Session reconfiguration triggers
        void networkOnlineStateChanged(bool online);
        void networkConfigurationChange(const QNetworkConfiguration &);

    private:
        struct MoveStorageJob
        {
            TorrentHandle *torrent;
            QString path;
            MoveStorageMode mode;
        };

        struct RemovingTorrentData
        {
            QString name;
            QString savePathToRemove;
            DeleteOption deleteOption;
        };

        explicit Session(QObject *parent = nullptr);
        ~Session();

        bool hasPerTorrentRatioLimit() const;
        bool hasPerTorrentSeedingTimeLimit() const;

        void initResumeFolder();

        // Session configuration
        void configure();
        void configureTorrents();
        void initializeNativeSession();
        void loadLTSettings(lt::settings_pack &settingsPack);
        void configureNetworkInterfaces(lt::settings_pack &settingsPack);
        void configurePeerClasses();
        void adjustLimits(lt::settings_pack &settingsPack);
        void applyBandwidthLimits(lt::settings_pack &settingsPack) const;
        void initMetrics();
        void adjustLimits();
        void applyBandwidthLimits();
        void processBannedIPs(lt::ip_filter &filter);
        QStringList getListeningIPs() const;
        void enableTracker(bool enable);
        void enableBandwidthScheduler();
        void populateAdditionalTrackers();
        void configureIPFilter();

        bool addTorrent_impl(CreateTorrentParams params, const MagnetUri &magnetUri,
                             TorrentInfo torrentInfo = TorrentInfo(),
                             const QByteArray &fastresumeData = {});
        bool findIncompleteFiles(TorrentInfo &torrentInfo, QString &savePath) const;

        void updateSeedingLimitTimer();
        void exportTorrentFile(TorrentHandle *const torrent, TorrentExportFolder folder = TorrentExportFolder::Regular);

        void handleAlert(const lt::alert *a);
        void dispatchTorrentAlert(const lt::alert *a);
        void handleAddTorrentAlert(const lt::add_torrent_alert *p);
        void handleStateUpdateAlert(const lt::state_update_alert *p);
        void handleMetadataReceivedAlert(const lt::metadata_received_alert *p);
        void handleFileErrorAlert(const lt::file_error_alert *p);
        void handleReadPieceAlert(const lt::read_piece_alert *p) const;
        void handleTorrentRemovedAlert(const lt::torrent_removed_alert *p);
        void handleTorrentDeletedAlert(const lt::torrent_deleted_alert *p);
        void handleTorrentDeleteFailedAlert(const lt::torrent_delete_failed_alert *p);
        void handlePortmapWarningAlert(const lt::portmap_error_alert *p);
        void handlePortmapAlert(const lt::portmap_alert *p);
        void handlePeerBlockedAlert(const lt::peer_blocked_alert *p);
        void handlePeerBanAlert(const lt::peer_ban_alert *p);
        void handleUrlSeedAlert(const lt::url_seed_alert *p);
        void handleListenSucceededAlert(const lt::listen_succeeded_alert *p);
        void handleListenFailedAlert(const lt::listen_failed_alert *p);
        void handleExternalIPAlert(const lt::external_ip_alert *p);
        void handleSessionStatsAlert(const lt::session_stats_alert *p);
#if (LIBTORRENT_VERSION_NUM >= 10200)
        void handleAlertsDroppedAlert(const lt::alerts_dropped_alert *p) const;
#endif
        void handleStorageMovedAlert(const lt::storage_moved_alert *p);
        void handleStorageMovedFailedAlert(const lt::storage_moved_failed_alert *p);

        void createTorrentHandle(const lt::torrent_handle &nativeHandle);

        void saveResumeData();
        void saveTorrentsQueue();
        void removeTorrentsQueue();

        std::vector<lt::alert *> getPendingAlerts(lt::time_duration time = lt::time_duration::zero()) const;

        void moveTorrentStorage(const MoveStorageJob &job) const;
        void handleMoveTorrentStorageJobFinished(const QString &errorMessage = {});

        // BitTorrent
        lt::session *m_nativeSession = nullptr;

        bool m_deferredConfigureScheduled = false;
        bool m_listenInterfaceConfigured = false;

        const bool m_wasPexEnabled;

        int m_numResumeData = 0;
        int m_extraLimit = 0;
        QVector<BitTorrent::TrackerEntry> m_additionalTrackerList;
        QString m_resumeFolderPath;
        QFile *m_resumeFolderLock = nullptr;

        QTimer *m_refreshTimer = nullptr;
        QTimer *m_seedingLimitTimer = nullptr;
        QTimer *m_resumeDataTimer = nullptr;
        Statistics *m_statistics = nullptr;
        // IP filtering
        QPointer<FilterParserThread> m_filterParser;
        QPointer<BandwidthScheduler> m_bwScheduler;
        // Tracker
        QPointer<Tracker> m_tracker;
        // fastresume data writing thread
        QThread *m_ioThread = nullptr;
        ResumeDataSavingManager *m_resumeDataSavingManager = nullptr;

        QHash<InfoHash, TorrentInfo> m_loadedMetadata;
        QHash<InfoHash, TorrentHandle *> m_torrents;
        QHash<InfoHash, CreateTorrentParams> m_addingTorrents;
        QHash<QString, AddTorrentParams> m_downloadedTorrents;
        QHash<InfoHash, RemovingTorrentData> m_removingTorrents;
        QStringMap m_categories;
        QSet<QString> m_tags;

        // I/O errored torrents
        QSet<InfoHash> m_recentErroredTorrents;
        QTimer *m_recentErroredTorrentsTimer = nullptr;

        SessionMetricIndices m_metricIndices;
        lt::time_point m_statsLastTimestamp = lt::clock_type::now();

        SessionStatus m_status;
        CacheStatus m_cacheStatus;

        QNetworkConfigurationManager *m_networkManager = nullptr;

        QStringList m_bannedIPs;

        QList<MoveStorageJob> m_moveStorageQueue;

        static Session *m_instance;
    };
}

#endif // BITTORRENT_SESSION_H

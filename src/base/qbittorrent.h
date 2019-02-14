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

#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include "cmdoptions.h"
#include "types.h"

#ifndef DISABLE_WEBUI
class WebUI;
#endif

class FileLogger;
class IconProvider;
class Profile;

namespace BitTorrent
{
    struct AddTorrentParams;
    class TorrentHandle;
}

namespace RSS
{
    class Manager;
    class AutoDownloader;
}

class QBittorrent : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QBittorrent)

public:
    QBittorrent(QCoreApplication &app);
    ~QBittorrent() override;

    int run();

    const Profile *profile() const;
    const IconProvider *iconProvider() const;

    virtual void addTorrent(const QString &source, const BitTorrent::AddTorrentParams &torrentParams);

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

protected:
    virtual bool createComponents();
    virtual void beginCleanup();
    virtual void endCleanup();

    CommandLineParameters commandLineParameters() const;

    ShutdownAction m_shutdownAction = ShutdownAction::Exit;

private:
    virtual bool confirmShutdown() const;
    virtual void showStartupInfo() const;
    virtual void showErrorMessage(const QString &message) const;
    virtual void activate() const;
    virtual bool userAgreesWithLegalNotice();
    virtual void displayUsage() const;
    virtual void displayVersion() const;
    virtual IconProvider *createIconProvider();
#ifndef DISABLE_WEBUI
    virtual WebUI *createWebUI();
#endif

    void initializeTranslation();
    void processParams(const QStringList &params);
    void runExternalProgram(const BitTorrent::TorrentHandle *torrent) const;
    void sendNotificationEmail(const BitTorrent::TorrentHandle *torrent);

    void processMessage(const QString &message);
    void torrentFinished(BitTorrent::TorrentHandle *const torrent);
    void allTorrentsFinished();
    void cleanup();

    bool m_running = false;

    QTranslator *m_qtTranslator;
    QTranslator *m_translator;

    const CommandLineParameters m_commandLineParameters;

    Profile *m_profile;
    IconProvider *m_iconProvider;
#ifndef DISABLE_WEBUI
    WebUI *m_webui = nullptr;
#endif
    QPointer<FileLogger> m_fileLogger;
    QStringList m_paramsQueue;
};

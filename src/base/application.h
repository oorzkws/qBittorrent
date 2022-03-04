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

#pragma once

#include <QCoreApplication>
#include <QObject>
#include <QTranslator>

#include "cmdoptions.h"
#include "pathfwd.h"
#include "settingvalue.h"
#include "types.h"

class ApplicationInstanceManager;
class FileLogger;

namespace BitTorrent
{
    class Torrent;
    struct AddTorrentParams;
}

namespace RSS
{
    class Session;
    class AutoDownloader;
}

#ifndef DISABLE_WEBUI
class WebUI;
#endif

class Application : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)

public:
    Application(int &argc, char **argv);
    virtual ~Application();

#ifdef Q_OS_WIN
    int memoryWorkingSetLimit() const;
    void setMemoryWorkingSetLimit(int size);
#endif

    // FileLogger properties
    bool isFileLoggerEnabled() const;
    void setFileLoggerEnabled(bool value);
    Path fileLoggerPath() const;
    void setFileLoggerPath(const Path &path);
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

    const CommandLineParameters &commandLineArgs() const;
    bool isRunning();

    int exec(const QStringList &params);

    bool sendParams(const QStringList &params);

    virtual addTorrent(const QString &torrentSource, const BitTorrent::AddTorrentParams &torrentParams);

protected:
    explicit Application(QCoreApplication *qtApp);

    QCoreApplication *qtApp() const;

    virtual void activate();
    virtual bool initializeComponents();
    virtual bool confirmAutoExit(ShutdownDialogAction action) const;
    virtual bool processParam(QStringView param, BitTorrent::AddTorrentParams &torrentParams) const;

protected slots:
    virtual void cleanup();

private slots:
    void processMessage(const QString &message);
    void torrentFinished(BitTorrent::Torrent *const torrent);
    void allTorrentsFinished();

private:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void initializeTranslation();
    // As program parameters, we can get paths or urls.
    // This function parse the parameters and call
    // the right addTorrent function, considering
    // the parameter type.
    void processParams(const QStringList &params);
    void runExternalProgram(const BitTorrent::Torrent *torrent) const;
    void sendNotificationEmail(const BitTorrent::Torrent *torrent);
#ifdef Q_OS_WIN
    void applyMemoryWorkingSetLimit();
#endif

    QCoreApplication *m_qtApp = nullptr;
    QTranslator m_qtTranslator;
    QTranslator m_translator;

    CommandLineParameters m_commandLineArgs;
    QStringList m_paramsQueue;
    ShutdownDialogAction m_shutdownAct = ShutdownDialogAction::Exit;

    ApplicationInstanceManager *m_instanceManager = nullptr;
    bool m_running = false;
    FileLogger *m_fileLogger = nullptr;
#ifndef DISABLE_WEBUI
    WebUI *m_webui = nullptr;
#endif

#ifdef Q_OS_WIN
    SettingValue<int> m_storeMemoryWorkingSetLimit;
#endif
    SettingValue<bool> m_storeFileLoggerEnabled;
    SettingValue<bool> m_storeFileLoggerBackup;
    SettingValue<bool> m_storeFileLoggerDeleteOld;
    SettingValue<int> m_storeFileLoggerMaxSize;
    SettingValue<int> m_storeFileLoggerAge;
    SettingValue<int> m_storeFileLoggerAgeType;
    SettingValue<Path> m_storeFileLoggerPath;
};

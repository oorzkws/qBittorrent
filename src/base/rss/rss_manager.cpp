/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2010  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "rss_manager.h"

#include "base/settingsstorage.h"
#include "rss_item.h"
#include "rss_feed.h"
#include "private/rss_session.h"

const QString KEY_PROCESSING_ENABLED(QStringLiteral("RSS/Session/EnableProcessing"));
const QString KEY_REFRESH_INTERVAL(QStringLiteral("RSS/Session/RefreshInterval"));
const QString KEY_MAX_ARTICLES_PER_FEED(QStringLiteral("RSS/Session/MaxArticlesPerFeed"));

QPointer<RSS::Manager> RSS::Manager::m_instance = nullptr;

RSS::Manager::Manager()
    : m_processingEnabled(SettingsStorage::instance()->loadValue(KEY_PROCESSING_ENABLED, false).toBool())
    , m_refreshInterval(SettingsStorage::instance()->loadValue(KEY_REFRESH_INTERVAL, 30).toUInt())
    , m_maxArticlesPerFeed(SettingsStorage::instance()->loadValue(KEY_MAX_ARTICLES_PER_FEED, 50).toInt())
    , m_session(new Private::Session(m_refreshInterval, m_maxArticlesPerFeed, this))
{
    Q_ASSERT(!m_instance); // only one instance is allowed
    m_instance = this;

    connect(m_session, &Private::Session::itemAdded, this, &Manager::itemAdded);
    connect(m_session, &Private::Session::itemPathChanged, this, &Manager::itemPathChanged);
    connect(m_session, &Private::Session::itemAboutToBeRemoved, this, &Manager::itemAboutToBeRemoved);
    connect(m_session, &Private::Session::feedStateChanged, this, &Manager::feedStateChanged);

    // Remove legacy/corrupted settings
    // (at least on Windows, QSettings is case-insensitive and it can get
    // confused when asked about settings that differ only in their case)
    auto settingsStorage = SettingsStorage::instance();
    settingsStorage->removeValue("Rss/streamList");
    settingsStorage->removeValue("Rss/streamAlias");
    settingsStorage->removeValue("Rss/open_folders");
    settingsStorage->removeValue("Rss/qt5/splitter_h");
    settingsStorage->removeValue("Rss/qt5/splitterMain");
    settingsStorage->removeValue("Rss/hosts_cookies");
    settingsStorage->removeValue("RSS/streamList");
    settingsStorage->removeValue("RSS/streamAlias");
    settingsStorage->removeValue("RSS/open_folders");
    settingsStorage->removeValue("RSS/qt5/splitter_h");
    settingsStorage->removeValue("RSS/qt5/splitterMain");
    settingsStorage->removeValue("RSS/hosts_cookies");
    settingsStorage->removeValue("Rss/Session/EnableProcessing");
    settingsStorage->removeValue("Rss/Session/RefreshInterval");
    settingsStorage->removeValue("Rss/Session/MaxArticlesPerFeed");
    settingsStorage->removeValue("Rss/AutoDownloader/EnableProcessing");
}

RSS::Manager *RSS::Manager::instance()
{
    return m_instance;
}

bool RSS::Manager::addFolder(const QString &path, QString *error)
{
    return m_session->addFolder(path, error);
}

bool RSS::Manager::addFeed(const QString &url, const QString &path, QString *error)
{
    return m_session->addFeed(url, path, error);
}

void RSS::Manager::moveItem(Item &item, Folder &destFolder, const QString &name)
{
    m_session->moveItem(item, destFolder, name);
}

void RSS::Manager::removeItem(Item &item)
{
    m_session->removeItem(item);
}

bool RSS::Manager::moveItem(const QString &itemPath, const QString &destPath, QString *error)
{
    return m_session->moveItem(itemPath, destPath, error);
}

bool RSS::Manager::removeItem(const QString &itemPath, QString *error)
{
    return m_session->removeItem(itemPath, error);
}

QList<RSS::Item *> RSS::Manager::items() const
{
    return m_session->items();
}

RSS::Item *RSS::Manager::itemByID(const qint64 id) const
{
    return m_session->itemByID(id);
}

RSS::Item *RSS::Manager::itemByPath(const QString &path) const
{
    return m_session->itemByPath(path);
}

void RSS::Manager::refreshItem(qint64 itemId)
{
    m_session->refreshItem(itemId);
}

bool RSS::Manager::isProcessingEnabled() const
{
    return m_processingEnabled;
}

void RSS::Manager::setProcessingEnabled(bool enabled)
{
    if (m_processingEnabled != enabled) {
        m_processingEnabled = enabled;
        SettingsStorage::instance()->storeValue(KEY_PROCESSING_ENABLED, m_processingEnabled);
        m_session->setRefreshInterval(m_processingEnabled ? m_refreshInterval : 0);

        emit processingStateChanged(m_processingEnabled);
    }
}

RSS::Folder *RSS::Manager::rootFolder() const
{
    return m_session->rootFolder();
}

QVector<RSS::Feed *> RSS::Manager::feeds() const
{
    return m_session->feeds();
}

RSS::Feed *RSS::Manager::feedByURL(const QString &url) const
{
    return m_session->feedByURL(url);
}

uint RSS::Manager::refreshInterval() const
{
    return m_refreshInterval;
}

void RSS::Manager::setRefreshInterval(uint refreshInterval)
{
    if (m_refreshInterval != refreshInterval) {
        SettingsStorage::instance()->storeValue(KEY_REFRESH_INTERVAL, refreshInterval);
        m_refreshInterval = refreshInterval;
        m_session->setRefreshInterval(m_refreshInterval);
    }
}

RSS::Folder *RSS::Manager::addFolder(const QString &name)
{
    return addFolder(name, *rootFolder());
}

RSS::Folder *RSS::Manager::addFolder(const QString &name, Folder &destFolder)
{
    return m_session->addFolder(name, destFolder);
}

RSS::Feed *RSS::Manager::addFeed(const QString &url, const QString &name)
{
    return addFeed(url, name, *rootFolder());
}

RSS::Feed *RSS::Manager::addFeed(const QString &url, const QString &name, Folder &destFolder)
{
    return m_session->addFeed(url, name, destFolder);
}

void RSS::Manager::renameItem(Item &item, const QString &name)
{
    m_session->renameItem(item, name);
}

int RSS::Manager::maxArticlesPerFeed() const
{
    return m_maxArticlesPerFeed;
}

void RSS::Manager::setMaxArticlesPerFeed(int n)
{
    if (m_maxArticlesPerFeed != n) {
        m_maxArticlesPerFeed = n;
        SettingsStorage::instance()->storeValue(KEY_MAX_ARTICLES_PER_FEED, n);

        m_session->setMaxArticlesPerFeed(n);

        emit maxArticlesPerFeedChanged(n);
    }
}

void RSS::Manager::refreshAll()
{
    m_session->refreshAll();
}

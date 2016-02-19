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

#include "searchengine.h"

#include <QDir>
#include <QDebug>

#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "private/searchworker.h"
#include "private/luastate.h"

const QString UPDATE_URL = "https://raw.github.com/qbittorrent/qBittorrent/master/src/base/search/plugins/search";
const QString SHIPPED_PLUGINS_PATH = ":/plugins/search/";

struct Plugin: public PluginInfo
{
    QByteArray source;
};

namespace
{
    bool parseVersionInfo(const QByteArray &info, QHash<QString, qreal> &versionInfo)
    {
        versionInfo.clear();
        bool dataCorrect = false;

        QList<QByteArray> lines = info.split('\n');
        foreach (QByteArray line, lines) {
            line = line.trimmed();
            if (line.isEmpty()) continue;
            if (line.startsWith("#")) continue;

            QList<QByteArray> list = line.split(' ');
            if (list.size() != 2) continue;

            QString pluginName = QString(list.first());
            if (!pluginName.endsWith(":")) continue;

            pluginName.chop(1); // remove trailing ':'
            bool ok;
            qreal version = list.last().toFloat(&ok);
            qDebug("read line %s: %.2f", qPrintable(pluginName), version);
            if (!ok) continue;

            dataCorrect = true;
            versionInfo[pluginName] = version;
        }

        return dataCorrect;
    }

    static Plugin *load(const QString &name, const QByteArray &data)
    {
        LuaState luaState;
        if (!luaState.isValid())
            return nullptr;

        if (!luaState.load(data))
        {
            qCritical() << "Could not load plugin:"
                        << luaState.getValue().toString().toUtf8().constData();
            return nullptr;
        }

        if (luaState.getGlobal("name") != LuaType::String) {
            qCritical() << "Could not get plugin name.";
            return nullptr;
        }

        QString fullName = luaState.getValue().toString();
        luaState.pop();

        if (luaState.getGlobal("version") != LuaType::Number) {
            qCritical() << "Could not get plugin version.";
            return nullptr;
        }

        qreal version = luaState.getValue().toReal();
        luaState.pop();

        if (luaState.getGlobal("url") != LuaType::String) {
            qCritical() << "Could not get plugin url.";
            return nullptr;
        }

        QString url = luaState.getValue().toString();
        luaState.pop();

        if (luaState.getGlobal("supportedCategories") != LuaType::Table) {
            qCritical() << "Could not get plugin supported categories.";
            return nullptr;
        }

        QStringList supportedCategories = luaState.getValue().toHash().keys();
        luaState.pop();

        if (luaState.getGlobal("run") != LuaType::Function) {
            qCritical() << "Could not find run() method.";
            return nullptr;
        }

        qDebug() << "Search Plugin" << fullName
                 << "version" << QString("%1").arg(version, 2, 'f', 2).toUtf8().constData()
                 << "loaded.";
        qDebug() << "=> Url:" << url;
        qDebug() << "=> Supported Categories:" << supportedCategories;

        Plugin *plugin = new Plugin;
        plugin->name = name;
        plugin->version = version;
        plugin->fullName = fullName;
        plugin->url = url;
        plugin->supportedCategories = supportedCategories;
        plugin->source = data;
        plugin->enabled = !Preferences::instance()->getSearchEngDisabled().contains(name);
        return plugin;
    }

    static Plugin *loadFromFile(const QString &name, const QString &filename)
    {
        QFile file(filename);
        if (!file.open(QFile::ReadOnly)) {
            qCritical() << "Could not open plugin file:" << file.errorString();
            return nullptr;
        }

        return load(name, file.readAll());
    }
}

const QHash<QString, QString> SearchEngine::m_categoryNames = SearchEngine::initializeCategoryNames();

SearchEngine::SearchEngine()
{
    qRegisterMetaType<SearchResult>("SearchResult");

    loadPlugins();
    updateShippedPlugins();

    m_searchTimeout = new QTimer(this);
    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

SearchEngine::~SearchEngine()
{
    cancelSearch();
    qDebug("Search Engine destructed.");
}

QStringList SearchEngine::allPlugins() const
{
    return m_plugins.keys();
}

QStringList SearchEngine::enabledPlugins() const
{
    return m_enabledPlugins.keys();
}

QStringList SearchEngine::supportedCategories() const
{
    QStringList result;
    foreach (const Plugin *plugin, m_enabledPlugins) {
        foreach (QString cat, plugin->supportedCategories) {
            if (!result.contains(cat))
                result << cat;
        }
    }

    return result;
}

PluginInfo *SearchEngine::pluginInfo(const QString &name) const
{
    return m_plugins.value(name);
}

bool SearchEngine::isActive() const
{
    return !m_activeTasks.isEmpty();
}

void SearchEngine::enablePlugin(const QString &name, bool enabled)
{
    auto plugin = m_plugins.value(name);
    if (plugin) {
        Preferences *const pref = Preferences::instance();
        QStringList disabledPlugins = pref->getSearchEngDisabled();
        if (enabled && !m_enabledPlugins.contains(name)) {
            m_enabledPlugins[name] = plugin;
            disabledPlugins.removeAll(name);
        }
        else if (!disabledPlugins.contains(name)) {
            m_enabledPlugins.remove(name);
            disabledPlugins.append(name);
        }
        pref->setSearchEngDisabled(disabledPlugins);
    }
}

// Updates shipped plugin
void SearchEngine::updatePlugin(const QString &name)
{
    installPlugin(QString("%1/%2.lua").arg(UPDATE_URL).arg(name));
}

// Install or update plugin from file or url
void SearchEngine::installPlugin(const QString &source)
{
    qDebug("Asked to install plugin at %s", qPrintable(source));

    if (Utils::Misc::isUrl(source)) {
        Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(source, true);
        connect(handler, SIGNAL(downloadFinished(Net::DownloadHandler*)), this, SLOT(pluginDownloaded(Net::DownloadHandler*)));
    }
    else {
        QString path = source;
        if (path.startsWith("file:", Qt::CaseInsensitive))
            path = QUrl(path).toLocalFile();

        QString pluginName = Utils::Fs::fileName(path);
        pluginName.chop(pluginName.size() - pluginName.lastIndexOf("."));

        if (!path.endsWith(".lua", Qt::CaseInsensitive))
            emit pluginInstallationFailed(pluginName, tr("Unknown search plugin file format."));
        else
            installPlugin_impl(pluginName, path);
    }
}

void SearchEngine::installPlugin_impl(const QString &name, const QString &path)
{
    auto plugin = m_plugins.value(name);
    auto newPlugin = loadFromFile(name, path);
    if (!newPlugin) {
        if (plugin)
            emit pluginUpdateFailed(name, tr("Could not load plugin."));
        else
            emit pluginInstallationFailed(name, tr("Could not load plugin."));
        return;
    }

    if (plugin && (plugin->version >= newPlugin->version)) {
        qDebug("Apparently update is not needed, we have a more recent version");
        emit pluginUpdateFailed(name, tr("A more recent version of this plugin is already installed."));
        delete newPlugin;
        return;
    }

    // Process with install
    QString destPath = pluginPath(name);
    bool bakupFileCreated = false;
    if (QFile::exists(destPath)) {
        // Backup in case install fails
        QFile::copy(destPath, destPath + ".bak");
        Utils::Fs::forceRemove(destPath);
        bakupFileCreated = true;
    }

    if (!QFile::copy(path, destPath)) {
        // Remove broken file
        Utils::Fs::forceRemove(destPath);
        if (bakupFileCreated) {
            // restore backup
            QFile::copy(destPath + ".bak", destPath);
            Utils::Fs::forceRemove(destPath + ".bak");
            emit pluginUpdateFailed(name, tr("I/O Error."));
        }
        else {
            emit pluginInstallationFailed(name, tr("I/O Error."));
        }

        delete newPlugin;
    }
    else {
        addPlugin(name, newPlugin);

        // Install was successful, remove backup
        if (bakupFileCreated)
            Utils::Fs::forceRemove(destPath + ".bak");

        if (plugin) {
            delete plugin;
            emit pluginUpdated(name);
        }
        else {
            emit pluginInstalled(name);
        }
    }
}

bool SearchEngine::uninstallPlugin(const QString &name)
{
    if (QFile::exists(SHIPPED_PLUGINS_PATH + name + ".lua"))
        return false;

    QDir pluginsFolder(pluginsLocation());
    QStringList filter(QString("%1.*").arg(name));
    QStringList files = pluginsFolder.entryList(filter, QDir::Files, QDir::Unsorted);
    foreach (const QString &file, files)
        Utils::Fs::forceRemove(pluginsFolder.absoluteFilePath(file));

    m_enabledPlugins.take(name);
    delete m_plugins.take(name);

    return true;
}

void SearchEngine::checkForUpdates()
{
    // Download version file from update server on sourceforge
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(UPDATE_URL + "/versions.txt");
    connect(handler, SIGNAL(downloadFinished(Net::DownloadHandler*)), this, SLOT(versionInfoDownloaded(Net::DownloadHandler*)));
}

void SearchEngine::cancelSearch()
{
    if (m_activeTasks.isEmpty()) return;

    foreach (SearchWorker *task, m_activeTasks) {
        task->disconnect(this);
        task->cancel();
    }

    m_activeTasks.clear();
    emit searchFinished(true);
}

void SearchEngine::downloadTorrent(const QString &siteUrl, const QString &url)
{

}

void SearchEngine::startSearch(const QString &pattern, const QString &category, const QStringList &usedPlugins)
{
    // Search process already running or
    // No search pattern entered
    if (!m_activeTasks.isEmpty() || pattern.isEmpty()) {
        emit searchFailed();
        return;
    }

    // Launch search
    foreach (const QString &pluginName, usedPlugins) {
        Plugin *plugin = m_enabledPlugins.value(pluginName);
        Q_ASSERT(plugin);

        auto task = new SearchWorker {plugin->source, pattern,
                    plugin->supportedCategories.contains(category) ? category : "all"};
        connect(task, SIGNAL(finished()), SLOT(taskFinished()));
        connect(task, SIGNAL(finished()), task, SLOT(deleteLater()));
        connect(task, SIGNAL(newResult(SearchResult)), SLOT(handleNewResult(SearchResult)));
        task->start();

        m_activeTasks.append(task);
        if (m_activeTasks.size() == 1)
            emit searchStarted();
    }

    m_searchTimeout->start(180000); // 3min
}

QString SearchEngine::categoryFullName(const QString &categoryName)
{
    return tr(m_categoryNames.value(categoryName).toUtf8().constData());
}

QString SearchEngine::pluginsLocation()
{
    const QString location = Utils::Fs::expandPathAbs(
                QString("%1/plugins/search")
                .arg(Utils::Fs::QDesktopServicesDataLocation()));
    QDir locationDir(location);
    if (!locationDir.exists())
        locationDir.mkpath(locationDir.absolutePath());
    return location;
}

void SearchEngine::versionInfoDownloaded(Net::DownloadHandler *downloadHandler)
{
    downloadHandler->deleteLater();

    QHash<QString, qreal> updateInfo;
    if (downloadHandler->error() != Net::DownloadHandler::NoError) {
        emit checkForUpdatesFailed(tr("Update server is temporarily unavailable. %1").arg(downloadHandler->errorString()));
    }
    else if (!parseVersionInfo(downloadHandler->data(), updateInfo)) {
        emit checkForUpdatesFailed(tr("An incorrect update info received."));
    }
    else {
        foreach (const QString &pluginName, updateInfo.keys()) {
            if (m_plugins.contains(pluginName) && (updateInfo[pluginName] <= m_plugins[pluginName]->version))
                updateInfo.remove(pluginName);
        }
        emit checkForUpdatesFinished(updateInfo);
    }
}

void SearchEngine::pluginDownloaded(Net::DownloadHandler *downloadHandler)
{
    downloadHandler->deleteLater();

    if (downloadHandler->error() == Net::DownloadHandler::NoError) {
        QString filePath = Utils::Fs::fromNativePath(downloadHandler->filePath());

        QString pluginName = Utils::Fs::fileName(downloadHandler->url());
        pluginName.chop(pluginName.size() - pluginName.lastIndexOf(".")); // Remove extension
        installPlugin_impl(pluginName, filePath);
        Utils::Fs::forceRemove(filePath);
    }
    else {
        QString pluginName = downloadHandler->url().split('/').last();
        pluginName.replace(".lua", "", Qt::CaseInsensitive);
        if (m_plugins.contains(pluginName))
            emit pluginUpdateFailed(pluginName, tr("Failed to download the plugin file. %1").arg(downloadHandler->errorString()));
        else
            emit pluginInstallationFailed(pluginName, tr("Failed to download the plugin file. %1").arg(downloadHandler->errorString()));
    }
}

void SearchEngine::taskFinished()
{
    m_activeTasks.removeAll(static_cast<SearchWorker*>(sender()));
    if (m_activeTasks.isEmpty())
        emit searchFinished(false);
}

void SearchEngine::handleNewResult(SearchResult result)
{
    emit newSearchResults({result});
}

void SearchEngine::updateShippedPlugins()
{
    qDebug("Updating shipped plugins");

    QFile versionsFile(SHIPPED_PLUGINS_PATH + "versions.txt");
    if (!versionsFile.open(QFile::ReadOnly)) return;

    QHash<QString, qreal> versionInfo;
    if (!parseVersionInfo(versionsFile.readAll(), versionInfo)) return;

    QDir destDir(pluginsLocation());
    QDir shippedDir(SHIPPED_PLUGINS_PATH);
    foreach (const QString &pluginName, versionInfo.keys()) {
        if (!m_plugins.contains(pluginName) || (versionInfo[pluginName] > m_plugins[pluginName]->version))
            installPlugin_impl(pluginName, shippedDir.absoluteFilePath(pluginName + ".lua"));

        const QString iconFile = pluginName + ".png";
        if (!QFile::exists(destDir.absoluteFilePath(iconFile)))
            QFile::copy(shippedDir.absoluteFilePath(iconFile), destDir.absoluteFilePath(iconFile));
    }
}

void SearchEngine::onTimeout()
{
    cancelSearch();
}

void SearchEngine::addPlugin(const QString &pluginName, Plugin *plugin)
{
    m_plugins[pluginName] = plugin;
    if (plugin->enabled)
        m_enabledPlugins[pluginName] = plugin;
}

void SearchEngine::loadPlugins()
{
    QDir pluginsDir(pluginsLocation());
    QStringList filter("*.lua");
    foreach (const QString &file, pluginsDir.entryList(filter)) {
        const QString pluginName = file.left(file.size() - 4);
        Plugin *plugin = loadFromFile(pluginName, pluginsDir.absoluteFilePath(file));
        if (plugin)
            addPlugin(pluginName, plugin);
        else
            Logger::instance()->addMessage(QString("Could not load search plugin \"%1\"").arg(pluginName), Log::WARNING);
    }
}

bool SearchEngine::isUpdateNeeded(QString pluginName, qreal newVersion) const
{
    auto plugin = m_plugins.value(pluginName);
    if (!plugin) return true;

    qreal oldVersion = plugin->version;
    qDebug("IsUpdate needed? to be installed: %.2f, already installed: %.2f", newVersion, oldVersion);
    return (newVersion > oldVersion);
}

QString SearchEngine::pluginPath(const QString &name)
{
    return QString("%1/%2.lua").arg(pluginsLocation()).arg(name);
}

QHash<QString, QString> SearchEngine::initializeCategoryNames()
{
    QHash<QString, QString> result;

    result["all"] = QT_TRANSLATE_NOOP("SearchEngine", "All categories");
    result["movies"] = QT_TRANSLATE_NOOP("SearchEngine", "Movies");
    result["tv"] = QT_TRANSLATE_NOOP("SearchEngine", "TV shows");
    result["music"] = QT_TRANSLATE_NOOP("SearchEngine", "Music");
    result["games"] = QT_TRANSLATE_NOOP("SearchEngine", "Games");
    result["anime"] = QT_TRANSLATE_NOOP("SearchEngine", "Anime");
    result["software"] = QT_TRANSLATE_NOOP("SearchEngine", "Software");
    result["pictures"] = QT_TRANSLATE_NOOP("SearchEngine", "Pictures");
    result["books"] = QT_TRANSLATE_NOOP("SearchEngine", "Books");

    return result;
}

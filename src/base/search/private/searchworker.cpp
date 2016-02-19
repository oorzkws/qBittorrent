/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchworker.h"

#include <QByteArray>
#include <QDebug>

#include "luastate.h"
#include "luafunctions.h"

int newSearchResult(lua_State *state);

namespace
{
    qint64 anySizeToBytes(QString str)
    {
        static const quint64 KIBI = 1024;
        static const quint64 MIBI = KIBI * 1024;
        static const quint64 GIBI = MIBI * 1024;
        static const quint64 TIBI = GIBI * 1024;

        str = str.trimmed();

        // separate number from unit
        int i;
        for (i = str.size(); i > 0; --i) {
            if (!str[i - 1].isLetter()) break;
        }
        QString sizeStr = str.left(i).trimmed();
        QString unitStr = str.right(str.size() - i);

        bool ok;
        float size = sizeStr.toFloat(&ok);
        if (!ok) return -1;

        if (!unitStr.isEmpty()) {
            unitStr = unitStr[0].toUpper();
            unitStr == "K"
                    ? size *= KIBI : unitStr == "M"
                    ? size *= MIBI : unitStr == "G"
                    ? size *= GIBI : unitStr == "T"
                    ? size *= TIBI : size = -1;
        }

        return static_cast<qint64>(size);
    }
}

SearchWorker::SearchWorker(const QByteArray &source, const QString &pattern, const QString &category)
    : m_source(source)
    , m_pattern(pattern)
    , m_category(category)
    , m_cancelled(false)
{
}

bool SearchWorker::cancelled() const
{
    return m_cancelled;
}

void SearchWorker::handleNewResult(SearchResult result)
{
    emit newResult(result);
}

void SearchWorker::run()
{
    if (m_cancelled) return;

    LuaState luaState;
    if (!luaState.isValid() || !luaState.load(m_source))
        return;

    luaState.addBaseLib();
    luaState.addStringLib();
    luaState.addTableLib();

    luaState.push(this);
    luaState.setGlobal("__this__");

    luaState.push(newSearchResult);
    luaState.setGlobal("newSearchResult");
    luaState.push(trace);
    luaState.setGlobal("trace");

    // create JSON table and add methods to it
    luaState.push(QVariantMap());
    luaState.push(jsonLoad);
    luaState.setField(-2, "load");
    luaState.setGlobal("JSON");

    // create URL table and add methods to it
    luaState.push(QVariantMap());
    luaState.push(urlUrlencode);
    luaState.setField(-2, "urlencode");
    luaState.push(urlUnquote);
    luaState.setField(-2, "unquote");
    luaState.push(urlGet);
    luaState.setField(-2, "get");
    luaState.setGlobal("URL");

    // create HTML table and add methods to it
    luaState.push(QVariantMap());
    luaState.push(htmlParse);
    luaState.setField(-2, "parse");
    luaState.push(htmlUnescape);
    luaState.setField(-2, "unescape");
    luaState.setGlobal("HTML");

    // add string methods to global string table
    // (previously created when we load lua string library)
    luaState.getGlobal("string");
    luaState.push(stringStrip);
    luaState.setField(-2, "strip");
    luaState.push(stringSplit);
    luaState.setField(-2, "split");
    luaState.push(stringConvert);
    luaState.setField(-2, "convert");
    luaState.push(stringStartswith);
    luaState.setField(-2, "startswith");
    luaState.pop();

    luaState.getGlobal("run");
    luaState.push(m_pattern);
    luaState.push(m_category);
    if (!luaState.call(2, 0)) {
        qWarning() << "An error occured during plugin run:" << luaState.getValueAsString();
        luaState.pop();
    }
}

void SearchWorker::cancel()
{
    m_cancelled = true;
}

int newSearchResult(lua_State *state)
{
    LuaState luaState(state);

    int argn = luaState.getTop(); // number of arguments
    if ((argn != 1) || (luaState.getType(1) != LuaType::Table)) {
        luaState.push("newSearchResult(): bad arguments");
        luaState.raiseError();
    }

    if (luaState.getGlobal("__this__") != LuaType::LightUserData) {
        luaState.push("newSearchResult(): could not get SearchTask pointer");
        luaState.raiseError();
    }

    SearchWorker *task = reinterpret_cast<SearchWorker*>(luaState.getValue(-1).value<void*>());
    luaState.pop();
    if (task->cancelled()) {
        luaState.push("Search task was cancelled");
        luaState.raiseError();
    }

    QVariantHash rawResult = luaState.getValue(1).toHash();

    SearchResult result;
    result.fileName = QString::fromUtf8(rawResult.value("name").toByteArray().trimmed());
    result.fileUrl = QString::fromUtf8(rawResult.value("link").toByteArray().trimmed());
    result.fileSize = anySizeToBytes(QString::fromUtf8(rawResult.value("size").toByteArray().trimmed()));
    bool ok = false;
    result.nbSeeders = rawResult.value("seeds").toLongLong(&ok);
    if (!ok || (result.nbSeeders < 0))
        result.nbSeeders = -1;
    result.nbLeechers = rawResult.value("leeches").toLongLong(&ok);
    if (!ok || (result.nbLeechers < 0))
        result.nbLeechers = -1;
    result.siteUrl = QString::fromUtf8(rawResult.value("siteUrl").toByteArray().trimmed());
    result.descrLink = QString::fromUtf8(rawResult.value("descrLink").toByteArray().trimmed());
    task->handleNewResult(result);

    return 0;
}

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

#include "luafunctions.h"

#include <QDebug>
#include <QString>
#include <QStringList>
#include <QTextCodec>
#ifdef QBT_USES_QT5
#include <QUrlQuery>
#else
#include <QUrl>
#endif

#include "base/net/downloadmanager.h"
#include "base/net/downloadhandler.h"
#include "base/utils/json.h"
#include "base/htmlparser.h"
#include "luastate.h"

#define CHECK_HTML_HANDLER(name) m_##name = (m_luaState.getGlobal(#name) == LuaType::Function); m_luaState.pop();

namespace
{
    class PrivateHTMLParser: public HTMLParser
    {
    public:
        PrivateHTMLParser(lua_State *state)
            : m_luaState(state)
        {
            CHECK_HTML_HANDLER(handleStartTag);
            CHECK_HTML_HANDLER(handleEndTag);
            CHECK_HTML_HANDLER(handleData);
            CHECK_HTML_HANDLER(handleComment);
            CHECK_HTML_HANDLER(handleDecl);
            CHECK_HTML_HANDLER(handleUnknownDecl);
        }

    private:
        void handleStartTag(const QString &tag, const QHash<QString, QString> &attrs) override
        {
            if (!m_handleStartTag) return;

            m_luaState.getGlobal("handleStartTag");
            m_luaState.push(tag);
            m_luaState.push(attrs);
            if (!m_luaState.call(2, 0))
                m_luaState.raiseError();
        }

        void handleEndTag(const QString &tag) override
        {
            if (!m_handleEndTag) return;

            m_luaState.getGlobal("handleEndTag");
            m_luaState.push(tag);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        void handleData(const QString &data) override
        {
            if (!m_handleData) return;

            m_luaState.getGlobal("handleData");
            m_luaState.push(data);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        void handleComment(const QString &data) override
        {
            if (!m_handleComment) return;

            m_luaState.getGlobal("handleComment");
            m_luaState.push(data);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        void handleDecl(const QString &decl) override
        {
            if (!m_handleDecl) return;

            m_luaState.getGlobal("handleDecl");
            m_luaState.push(decl);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        void handlePI(const QString &data) override
        {
            if (!m_handlePI) return;

            m_luaState.getGlobal("handlePI");
            m_luaState.push(data);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        void handleUnknownDecl(const QString &data) override
        {
            if (!m_handleUnknownDecl) return;

            m_luaState.getGlobal("handleUnknownDecl");
            m_luaState.push(data);
            if (!m_luaState.call(1, 0))
                m_luaState.raiseError();
        }

        LuaState m_luaState;
        bool m_handleStartTag;
        bool m_handleEndTag;
        bool m_handleData;
        bool m_handleComment;
        bool m_handlePI;
        bool m_handleDecl;
        bool m_handleUnknownDecl;
    };
}

int trace(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if (argn < 1) {
        luaState.push("trace(): bad arguments");
        luaState.raiseError();
    }

    QString msg = luaState.getValueAsString(1);
    for (int i = 2; i <= argn; ++i)
        msg += " " + luaState.getValueAsString(i);

    qDebug() << msg.toUtf8().constData();
    return 0; // number of results
}

int stringStrip(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("strip(): bad arguments");
        luaState.raiseError();
    }

    luaState.push(luaState.getValueAsString(1).trimmed());
    return 1; // number of results
}

int stringSplit(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn < 2) || (argn > 3)
        || (luaState.getType(1) != LuaType::String)
        || (luaState.getType(2) != LuaType::String)
        || ((argn == 3) && (luaState.getType(3) != LuaType::Bool))) {
        luaState.push("split(): bad arguments");
        luaState.raiseError();
    }

    bool skipEmpty = argn == 3 ? luaState.getValue(3).toBool() : false;
    luaState.push(luaState.getValueAsString(1).split(
             luaState.getValueAsString(2),
             skipEmpty ? QString::SkipEmptyParts : QString::KeepEmptyParts));

    return 1; // number of results
}

int stringConvert(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 3)
        || (luaState.getType(1) != LuaType::String)
        || (luaState.getType(2) != LuaType::String)
        || (luaState.getType(3) != LuaType::String)) {
        luaState.push("convert(): bad arguments");
        luaState.raiseError();
    }

    QByteArray from = luaState.getValue(2).toByteArray();
    QByteArray to = luaState.getValue(3).toByteArray();

    auto srcCodec = QTextCodec::codecForName(from);
    if (!srcCodec) {
        luaState.push(QString("convert(): codec not found for %1").arg(from.constData()));
        luaState.raiseError();
    }

    auto destCodec = QTextCodec::codecForName(to);
    if (!destCodec) {
        luaState.push(QString("convert(): codec not found for %1").arg(to.constData()));
        luaState.raiseError();
    }

    luaState.push(destCodec->fromUnicode(srcCodec->toUnicode(luaState.getValue(1).toByteArray())));
    return 1; // number of results
}

int stringStartswith(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 2)
        || (luaState.getType(1) != LuaType::String)
        || (luaState.getType(2) != LuaType::String)) {
        luaState.push("startswith(): bad arguments");
        luaState.raiseError();
    }

    luaState.push(luaState.getValue(1).toByteArray().startsWith(luaState.getValue(2).toByteArray()));
    return 1; // number of results
}

int urlGet(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn <= 0) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("download(): bad arguments");
        luaState.raiseError();
    }

    QString url = luaState.getValueAsString(1);
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->downloadUrl(url);
    handler->waitForFinished();

    luaState.push(handler->data());
    delete handler;
    return 1; // number of results
}

int urlUrlencode(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::Table)) {
        luaState.push("urlencode(): bad arguments");
        luaState.raiseError();
    }

#ifdef QBT_USES_QT5
    QUrlQuery query;
#else
    QUrl query;
#endif

    QVariantHash items = luaState.getValue(1).toHash();
    foreach (const QString &key, items.keys())
        query.addQueryItem(key, QString::fromUtf8(items[key].toByteArray()));

    luaState.push(query.toString());
    return 1; // number of results
}

int jsonLoad(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("jsonLoad(): bad arguments");
        luaState.raiseError();
    }

    luaState.push(Utils::JSON::fromJson(luaState.getValueAsString(1)));
    return 1; // number of results
}

int htmlParse(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("parseHtml(): bad arguments");
        luaState.raiseError();
    }

    PrivateHTMLParser parser(state);
    try {
        parser.feed(luaState.getValue(1).toByteArray());
        parser.close();
    }
    catch (HTMLParseError &err) {
        luaState.push(err.message());
        luaState.raiseError();
    }

    return 0; // number of results
}

int urlUnquote(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("unquote(): bad arguments");
        luaState.raiseError();
    }

    luaState.push(QByteArray::fromPercentEncoding(luaState.getValue(1).toByteArray()));
    return 1; // number of results
}

int htmlUnescape(lua_State *state)
{
    LuaState luaState(state);
    int argn = luaState.getTop(); // number of arguments

    if ((argn != 1) || (luaState.getType(1) != LuaType::String)) {
        luaState.push("unescape(): bad arguments");
        luaState.raiseError();
    }

    luaState.push(HTMLParser::unescape(QString::fromUtf8(luaState.getValue(1).toByteArray())));
    return 1; // number of results
}

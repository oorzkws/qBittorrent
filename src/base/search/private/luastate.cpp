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

#include "luastate.h"

#include <lua.hpp>
#include <QDebug>
#include <QMetaType>
#include <QStringList>
#include <QVariant>

LuaState::LuaState(lua_State *luaState)
    : m_luaState(luaState)
    , m_needCleanup(luaState == nullptr)
{
    if (!m_luaState)
        m_luaState = luaL_newstate();

    if (!m_luaState)
        qCritical() << "Could not create Lua VM.";
}

LuaState::~LuaState()
{
    if (m_needCleanup) {
        qDebug() << "Closing Lua VM...";
        lua_close(m_luaState);
    }
}

bool LuaState::isValid() const
{
    return m_luaState;
}

int LuaState::getTop() const
{
    return lua_gettop(m_luaState);
}

LuaType LuaState::getType(int index) const
{
    switch (lua_type(m_luaState, index)) {
    case LUA_TNIL:
        return LuaType::Nil;
    case LUA_TBOOLEAN:
        return LuaType::Bool;
    case LUA_TNUMBER:
        return LuaType::Number;
    case LUA_TSTRING:
        return LuaType::String;
    case LUA_TLIGHTUSERDATA:
        return LuaType::LightUserData;
    case LUA_TUSERDATA:
        return LuaType::UserData;
    case LUA_TTABLE:
        return LuaType::Table;
    case LUA_TFUNCTION:
        return LuaType::Function;
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported data type");
        return LuaType::Unknown;
    }
}

QVariant LuaState::getValue(int index)
{
    switch (lua_type(m_luaState, index)) {
    case LUA_TNIL:
        return QVariant();
    case LUA_TBOOLEAN:
        return lua_toboolean(m_luaState, index);
    case LUA_TNUMBER:
        return lua_tonumber(m_luaState, index);
    case LUA_TSTRING:
        return QByteArray(lua_tostring(m_luaState, index));
    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
        return QVariant::fromValue(lua_touserdata(m_luaState, index));
    case LUA_TTABLE:
        return tableValue(index);
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported data type");
        return QVariant();
    }
}

QString LuaState::getValueAsString(int index)
{
    return QString::fromUtf8(getValue(index).toByteArray());
}

bool LuaState::load(const QByteArray &source)
{
    return ((luaL_loadstring(m_luaState, source.constData()) == LUA_OK)
            && (lua_pcall(m_luaState, 0, 0, 0) == LUA_OK));
}

void LuaState::addBaseLib()
{
    luaL_requiref(m_luaState, "_G", luaopen_base, 1);
    pop();
}

void LuaState::addStringLib()
{
    luaL_requiref(m_luaState, LUA_STRLIBNAME, luaopen_string, 1);
    pop();
}

void LuaState::addTableLib()
{
    luaL_requiref(m_luaState, LUA_TABLIBNAME, luaopen_table, 1);
    pop();
}

void LuaState::push(const char *string)
{
    lua_pushstring(m_luaState, string);
}

void LuaState::push(const QString &string)
{
    lua_pushstring(m_luaState, string.toUtf8().constData());
}

void LuaState::push(const QByteArray &string)
{
    lua_pushstring(m_luaState, string.constData());
}

void LuaState::push(const QStringList &stringList)
{
    pushList(stringList);
}

void LuaState::push(const QHash<QString, QString> &map)
{
    pushMap(map);
}

QVariant LuaState::tableValue(int index)
{
    QVariantHash table;

    index = lua_absindex(m_luaState, index);
    lua_pushnil(m_luaState);  // first key
    while (lua_next(m_luaState, index) != 0) {
        // uses 'key' (at index -2) and 'value' (at index -1)
        if (lua_type(m_luaState, -1) != LUA_TFUNCTION)
            // we don't support table items of function type here
            table[getValueAsString(-2)] = getValue(-1);
        // removes 'value'; keeps 'key' for next iteration
        pop();
    }
    pop();

    return table;
}

void LuaState::push(const QVariant &value)
{
    if (value.isNull()) {
        lua_pushnil(m_luaState);
        return;
    }

    switch (static_cast<QMetaType::Type>(value.userType())) {
    case QMetaType::Bool:
        lua_pushboolean(m_luaState, value.toBool());
        break;
    case QMetaType::Char:
    case QMetaType::UChar:
#ifdef QBT_USES_QT5
    case QMetaType::SChar:
#endif
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Int:
    case QMetaType::UInt:
        lua_pushinteger(m_luaState, value.toLongLong());
        break;
    case QMetaType::Float:
    case QMetaType::Double:
        lua_pushnumber(m_luaState, value.toDouble());
        break;
    case QMetaType::QChar:
    case QMetaType::QString:
        lua_pushstring(m_luaState, value.toString().toUtf8().constData());
        break;
    case QMetaType::QByteArray:
        lua_pushstring(m_luaState, value.toByteArray().constData());
        break;
    case QMetaType::QVariantMap:
        pushMap(value.toMap());
        break;
    case QMetaType::QVariantHash:
        pushMap(value.toHash());
        break;
    case QMetaType::QVariantList:
        pushList(value.toList());
        break;
    case QMetaType::QStringList:
        pushList(value.toStringList());
        break;
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Unsupported data type");
        lua_pushnil(m_luaState);
    }
}

void LuaState::pop(int count)
{
    lua_pop(m_luaState, count);
}

LuaType LuaState::getGlobal(const char *name)
{
    lua_getglobal(m_luaState, name);
    return getType();
}

LuaType LuaState::getGlobal(const QString &name)
{
    return getGlobal(name.toUtf8().constData());
}

void LuaState::setGlobal(const char *name)
{
    lua_setglobal(m_luaState, name);
}

void LuaState::setField(int index, const char *name)
{
    lua_setfield(m_luaState, index, name);
}

bool LuaState::call(int nargs, int nresults)
{
    return (lua_pcall(m_luaState, nargs, nresults, 0) == LUA_OK);
}

void LuaState::raiseError()
{
    lua_error(m_luaState);
}

void LuaState::raiseError(const char *msg)
{
    push(msg);
    raiseError();
}

void LuaState::push(void *pointer)
{
    lua_pushlightuserdata(m_luaState, pointer);
}

void LuaState::push(int (*func)(lua_State *))
{
    lua_pushcfunction(m_luaState, func);
}

template <typename List>
void LuaState::pushList(const List &list)
{
    lua_createtable(m_luaState, list.size(), 0);

    int i = 1;
    foreach (const auto &item, list) {
        push(item);
        lua_seti(m_luaState, -2, i++);
    }
}

template <typename Map>
void LuaState::pushMap(const Map &map)
{
    lua_createtable(m_luaState, 0, map.size());

    foreach (const QString &key, map.keys()) {
        push(map[key]);
        lua_setfield(m_luaState, -2, key.toUtf8().constData());
    }
}

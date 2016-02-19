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

#ifndef LUASTATE_H
#define LUASTATE_H

#include <QHash>

class QByteArray;
class QString;
class QStringList;
class QVariant;
struct lua_State;

enum class LuaType
{
    Unknown = -1,

    Nil,
    Bool,
    Number,
    String,
    Table,
    UserData,
    LightUserData,
    Function
};

class LuaState
{
public:
    explicit LuaState(lua_State *luaState = nullptr);
    ~LuaState();

    bool isValid() const;

    int getTop() const;
    LuaType getType(int index = -1) const;
    QVariant getValue(int index = -1);
    QString getValueAsString(int index = -1);

    bool load(const QByteArray &source);
    void addBaseLib();
    void addStringLib();
    void addTableLib();

    void push(const char *string);
    void push(const QString &string);
    void push(const QByteArray &string);
    void push(const QStringList &stringList);
    void push(const QHash<QString, QString> &map);
    void push(void *pointer);
    void push(int (*func)(lua_State *));
    void push(const QVariant &getValue);
    void pop(int count = 1);
    LuaType getGlobal(const char *name);
    LuaType getGlobal(const QString &name);
    void setGlobal(const char *name);
    void setField(int index, const char *name);

    bool call(int nargs, int nresults);
    void raiseError();
    void raiseError(const char *msg);

private:
    template <typename List> void pushList(const List &list);
    template <typename Map> void pushMap(const Map &map);

    QVariant tableValue(int index);

    lua_State *m_luaState;
    bool m_needCleanup;
};

#endif // LUASTATE_H

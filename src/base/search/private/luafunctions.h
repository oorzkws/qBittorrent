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

#ifndef LUAFUNCTIONS_H
#define LUAFUNCTIONS_H

struct lua_State;

// void trace(...)
int trace(lua_State *state);

// string string.strip(string text)
int stringStrip(lua_State *state);

// string string.split(string text, string sep, bool skipEmptyParts = false)
int stringSplit(lua_State *state);

// string string.convert(string text, string fromEncoding, string toEncoding)
int stringConvert(lua_State *state);

// string string.startswith(string text, string substr)
int stringStartswith(lua_State *state);

// string URL.get(string url)
int urlGet(lua_State *state);

// string URL.urlencode(table params)
int urlUrlencode(lua_State *state);

// string URL.unquote(string text)
int urlUnquote(lua_State *state);

// table JSON.load(string data)
int jsonLoad(lua_State *state);

// string HTML.unescape(string text)
int htmlUnescape(lua_State *state);

// void HTML.parse(string data)
int htmlParse(lua_State *state);

#endif // LUAFUNCTIONS_H

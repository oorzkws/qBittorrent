/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Vladimir Golovnev <glassez@yandex.ru>
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

#include <functional>
#include <type_traits>

#include <QMetaEnum>
#include <QString>

#include "settingsstorage.h"

template <typename T>
using ProxyFunc = std::function<T (const T &)>;

class PreferencesItemHandlerBase
{
public:
    bool isChanged() const;
    void reset();

protected:
    bool m_isChanged = false;
};

template <typename T>
class PreferencesItemHandler : public PreferencesItemHandlerBase
{
    Q_DISABLE_COPY(PreferencesItemHandler)

public:
    PreferencesItemHandler(SettingsStorage *storage, const QString &keyName
                         , const T &defaultValue, const ProxyFunc<T> &proxyFunc)
        : m_storage {storage}
        , m_keyName {keyName}
        , m_value {proxyFunc(loadValue(defaultValue))}
        , m_proxyFunc {proxyFunc}
    {
    }

    T get() const
    {
        return m_value;
    }

    void set(const T &newValue)
    {
        if (m_value == newValue)
            return;

        m_value = m_proxyFunc(newValue);
        storeValue(m_value);
        m_isChanged = true;
    }

private:
    // regular load/save pair
    template <typename U, typename std::enable_if<!std::is_enum<U>::value, int>::type = 0>
    U loadValue(const U &defaultValue)
    {
        return m_storage->loadValue(m_keyName, defaultValue).template value<T>();
    }

    template <typename U, typename std::enable_if<!std::is_enum<U>::value, int>::type = 0>
    void storeValue(const U &value)
    {
        m_storage->storeValue(m_keyName, value);
    }

    // load/save pair for an enum
    // saves literal value of the enum constant, obtained from QMetaEnum
    template <typename U, typename std::enable_if<std::is_enum<U>::value, int>::type = 0>
    U loadValue(const U &defaultValue)
    {
        static_assert(std::is_same<int, typename std::underlying_type<U>::type>::value,
                      "Enumeration underlying type has to be int");

        bool ok = false;
        const U res = static_cast<U>(QMetaEnum::fromType<U>().keyToValue(
            m_storage->loadValue(m_keyName).toString().toLatin1().constData(), &ok));
        return ok ? res : defaultValue;
    }

    template <typename U, typename std::enable_if<std::is_enum<U>::value, int>::type = 0>
    void storeValue(const U &value)
    {
        m_storage->storeValue(m_keyName,
            QString::fromLatin1(QMetaEnum::fromType<U>().valueToKey(static_cast<int>(value))));
    }

    SettingsStorage *m_storage;
    const QString m_keyName;
    T m_value;
    const ProxyFunc<T> m_proxyFunc;
};

class PreferencesBase : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PreferencesBase)

#define ITEM(name, type, key, ...) \
    PreferencesItemHandler<type> &name = registerItemHandler<type>(QStringLiteral(key), __VA_ARGS__)

public:
    explicit PreferencesBase(QObject *parent = nullptr);
    ~PreferencesBase() override;

    void notifyChanged();

signals:
    void changed();

protected:
    const QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    void setValue(const QString &key, const QVariant &value);

    template <typename T>
    PreferencesItemHandler<T> &registerItemHandler(const QString &keyName, const T &defaultValue = T {}, const ProxyFunc<T> &proxyFunc = [](const T &t) { return t; })
    {
        auto itemHandler = new PreferencesItemHandler<T> {m_storage, keyName, defaultValue, proxyFunc};
        m_itemHandlers << itemHandler;
        return *itemHandler;
    }

private:
    SettingsStorage *m_storage;
    QList<PreferencesItemHandlerBase *> m_itemHandlers;
};

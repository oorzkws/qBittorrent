/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtGlobal>

template <typename T>
class SharedDataPointer
{
public:
    SharedDataPointer() noexcept = default;

    explicit SharedDataPointer(T *data) noexcept
    {
        if (data)
        {
            m_data = new StorageType;
            m_data->data = data;
            m_data->ref.ref();
        }
    }

    SharedDataPointer(const SharedDataPointer &other) noexcept
        : m_data(other.m_data)
    {
        if (m_data)
            m_data->ref.ref();
    }

    SharedDataPointer(SharedDataPointer &&other) noexcept
        : m_data(qExchange(other.m_data, nullptr))
    {
    }

    ~SharedDataPointer()
    {
        if (m_data && !m_data->ref.deref())
            delete m_data;
    }

    SharedDataPointer &operator=(const SharedDataPointer &other) noexcept
    {
        if (other.m_data != m_data)
        {
            if (other.m_data)
                other.m_data->ref.ref();
            StorageType *oldData = qExchange(m_data, other.m_data);
            if (oldData && !oldData->ref.deref())
                delete oldData;
        }

        return *this;
    }

    inline SharedDataPointer &operator=(T *data) noexcept
    {
        reset(data);
        return *this;
    }

    SharedDataPointer &operator=(SharedDataPointer &&other) noexcept
    {
        SharedDataPointer moved = std::move(other);
        swap(moved);
        return *this;
    }

    T &operator*()
    {
        detach();
        return *m_data->data;
    }

    const T &operator*() const
    {
        return *m_data->data;
    }

    T *operator->()
    {
        detach();
        return m_data->data;
    }

    const T *operator->() const noexcept
    {
        return m_data->data;
    }

    operator T *()
    {
        detach();
        return m_data->data;
    }

    operator const T *() const noexcept
    {
        return m_data->data;
    }

    operator bool() const noexcept
    {
        return m_data != nullptr;
    }

    bool operator!() const noexcept
    {
        return m_data == nullptr;
    }

    T *data()
    {
        detach();
        return m_data->data;
    }

    const T *data() const noexcept
    {
        return m_data->data;
    }

    const T *constData() const noexcept
    {
        return m_data->data;
    }

    T *take() noexcept
    {
        return qExchange(m_data->data, nullptr);
    }

    void reset(T *data = nullptr) noexcept
    {
        if (data != m_data->data)
        {
            StorageType *newData = nullptr;
            if (data)
            {
                newData = new StorageType;
                newData->data = data;
                newData->ref.ref();
            }

            StorageType *oldData = qExchange(m_data, newData);
            if (oldData && !oldData->ref.deref())
                delete oldData;
        }
    }

    void detach()
    {
        if (m_data && (m_data->ref.loadRelaxed() != 1))
        {
            auto x = new StorageType(*m_data);
            x->ref.ref();
            if (!m_data->ref.deref())
                delete m_data;
            m_data = x;
        }
    }

    void swap(SharedDataPointer &other) noexcept
    {
        qSwap(m_data, other.m_data);
    }

//#define DECLARE_COMPARE_SET(T1, A1, T2, A2) \
//    friend bool operator<(T1, T2) noexcept \
//    { return std::less<T*>{}(A1, A2); } \
//    friend bool operator<=(T1, T2) noexcept \
//    { return !std::less<T*>{}(A2, A1); } \
//    friend bool operator>(T1, T2) noexcept \
//    { return std::less<T*>{}(A2, A1); } \
//    friend bool operator>=(T1, T2) noexcept \
//    { return !std::less<T*>{}(A1, A2); } \
//    friend bool operator==(T1, T2) noexcept \
//    { return A1 == A2; } \
//    friend bool operator!=(T1, T2) noexcept \
//    { return A1 != A2; } \

//    DECLARE_COMPARE_SET(const SharedDataPointer &p1, p1.d, const SharedDataPointer &p2, p2.d)
//    DECLARE_COMPARE_SET(const SharedDataPointer &p1, p1.d, const T *ptr, ptr)
//    DECLARE_COMPARE_SET(const T *ptr, ptr, const SharedDataPointer &p2, p2.d)
//    DECLARE_COMPARE_SET(const SharedDataPointer &p1, p1.d, std::nullptr_t, nullptr)
//    DECLARE_COMPARE_SET(std::nullptr_t, nullptr, const SharedDataPointer &p2, p2.d)

private:
    struct StorageType
    {
        T *data = nullptr;
        mutable QAtomicInt ref = 0;

        StorageType() noexcept = default;

        StorageType(const StorageType &other) noexcept
            : data(new T(*other.data))
            , ref(0)
        {
        }

        ~StorageType()
        {
            delete data;
        }

        // using the assignment operator would lead to corruption in the ref-counting
        StorageType &operator=(const StorageType &) = delete;
    };

    StorageType *m_data = nullptr;
};

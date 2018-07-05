/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>

#include "../rss_feed.h"

namespace RSS
{
    namespace Private
    {
        class FeedImpl : public Feed
        {
            Q_OBJECT
            Q_DISABLE_COPY(FeedImpl)

        public:
            FeedImpl(qint64 id, const QString &url, const QString &path, int maxArticles);
            ~FeedImpl() override = default;

            QList<Article *> articles() const override;
            int unreadCount() const override;
            void markAsRead() override;
            QString title() const override;
            QString lastBuildDate() const override;
            bool hasError() const override;
            bool isLoading() const override;
            Article *articleByGUID(const QString &guid) const override;

            bool isDirty() const;
            void setDirty(bool isDirty);
            void setHasError(bool hasError);
            void setLoading(bool isLoading);
            void setTitle(const QString &title);
            void setLastBuildDate(const QString &lastBuildDate);
            void setMaxArticles(int n);

            bool addArticle(Article *article);

        private:
            void handleArticleRead(Article *article);
            void increaseUnreadCount();
            void decreaseUnreadCount();
            void removeOldestArticle();

            QString m_title;
            QString m_lastBuildDate;
            bool m_hasError = false;
            bool m_isLoading = false;
            int m_maxArticles;
            QHash<QString, Article *> m_articles;
            QList<Article *> m_articlesByDate;
            int m_unreadCount = 0;
            QString m_dataFileName;
            bool m_isDirty = false;
        };
    }
}

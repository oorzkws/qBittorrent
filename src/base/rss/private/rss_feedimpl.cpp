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

#include "rss_feedimpl.h"

#include <QByteArray>

#include "base/global.h"
#include "../rss_article.h"

RSS::Private::FeedImpl::FeedImpl(qint64 id, const QString &url, const QString &path, int maxArticles)
    : Feed {id, url, path}
    , m_maxArticles {maxArticles}
{
}

QList<RSS::Article *> RSS::Private::FeedImpl::articles() const
{
    return m_articlesByDate;
}

int RSS::Private::FeedImpl::unreadCount() const
{
    return m_unreadCount;
}

void RSS::Private::FeedImpl::markAsRead()
{
    auto oldUnreadCount = m_unreadCount;
    for (auto *article : qAsConst(m_articles)) {
        if (!article->isRead()) {
            article->disconnect(this);
            article->markAsRead();
            --m_unreadCount;
            emit articleRead(article);
        }
    }

    if (m_unreadCount != oldUnreadCount) {
        m_isDirty = true;
        emit unreadCountChanged(this);
    }
}

QString RSS::Private::FeedImpl::title() const
{
    return m_title;
}

QString RSS::Private::FeedImpl::lastBuildDate() const
{
    return m_lastBuildDate;
}

bool RSS::Private::FeedImpl::hasError() const
{
    return m_hasError;
}

bool RSS::Private::FeedImpl::isLoading() const
{
    return m_isLoading;
}

RSS::Article *RSS::Private::FeedImpl::articleByGUID(const QString &guid) const
{
    return m_articles.value(guid);
}

void RSS::Private::FeedImpl::handleArticleRead(Article *article)
{
    article->disconnect(this);
    decreaseUnreadCount();
    emit articleRead(article);
    // will be stored deferred
    m_isDirty = true;
}

void RSS::Private::FeedImpl::increaseUnreadCount()
{
    ++m_unreadCount;
    emit unreadCountChanged(this);
}

void RSS::Private::FeedImpl::decreaseUnreadCount()
{
    Q_ASSERT(m_unreadCount > 0);

    --m_unreadCount;
    emit unreadCountChanged(this);
}

void RSS::Private::FeedImpl::setLastBuildDate(const QString &lastBuildDate)
{
    if (m_lastBuildDate != lastBuildDate) {
        m_lastBuildDate = lastBuildDate;
        setDirty(true);
    }
}

void RSS::Private::FeedImpl::setMaxArticles(int n)
{
    while (m_articles.size() > n)
        removeOldestArticle();
}

void RSS::Private::FeedImpl::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        setDirty(true);
    }
}

void RSS::Private::FeedImpl::setLoading(bool isLoading)
{
    m_isLoading = isLoading;
}

bool RSS::Private::FeedImpl::addArticle(Article *article)
{
    Q_ASSERT(article);
    Q_ASSERT(!m_articles.contains(article->localId()));

    // Insertion sort
    auto lowerBound = std::lower_bound(m_articlesByDate.begin(), m_articlesByDate.end()
                                       , article->date(), Article::articleDateRecentThan);
    if ((lowerBound - m_articlesByDate.begin()) >= m_maxArticles)
        return false; // we reach max articles

    m_articles[article->localId()] = article;
    m_articlesByDate.insert(lowerBound, article);
    if (!article->isRead()) {
        increaseUnreadCount();
        connect(article, &Article::read, this, &RSS::Private::FeedImpl::handleArticleRead);
    }

    setDirty(true);
    emit newArticle(article);

    if (m_articlesByDate.size() > m_maxArticles)
        removeOldestArticle();

    return true;
}

void RSS::Private::FeedImpl::removeOldestArticle()
{
    auto oldestArticle = m_articlesByDate.last();
    emit articleAboutToBeRemoved(oldestArticle);

    m_articles.remove(oldestArticle->localId());
    m_articlesByDate.removeLast();
    bool isRead = oldestArticle->isRead();
    delete oldestArticle;

    if (!isRead)
        decreaseUnreadCount();
}

void RSS::Private::FeedImpl::setHasError(bool hasError)
{
    m_hasError = hasError;
}

bool RSS::Private::FeedImpl::isDirty() const
{
    return m_isDirty;
}

void RSS::Private::FeedImpl::setDirty(bool dirty)
{
    m_isDirty = dirty;
}

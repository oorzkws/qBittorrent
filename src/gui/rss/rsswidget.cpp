/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017-2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2006  Arnaud Demaiziere <arnaud@qbittorrent.org>
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

#include "rsswidget.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDragMoveEvent>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QShortcut>
#include <QString>
#include <QTreeView>

#include "base/bittorrent/session.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_folder.h"
#include "base/rss/rss_session.h"
#include "addnewtorrentdialog.h"
#include "autoexpandabledialog.h"
#include "automatedrssdownloader.h"
#include "rssfeedmodel.h"
#include "rssfeedsortmodel.h"
#include "rssmodel.h"
#include "rsssortmodel.h"
#include "ui_rsswidget.h"
#include "uithememanager.h"

namespace
{
    RSS::Item *getItemPtr(const QModelIndex &index)
    {
        return index.data(RSSModel::ItemPtrRole).value<RSS::Item *>();
    }

    bool isStickyItem(const QModelIndex &index)
    {
        return (!index.parent().isValid() && (index.row() == 0));
    }

    bool isFolder(const QModelIndex &index)
    {
        return qobject_cast<RSS::Folder *>(getItemPtr(index));
    }

    RSS::Article *getArticlePtr(const QModelIndex &index)
    {
        return index.data(RSSFeedModel::ItemPtrRole).value<RSS::Article *>();
    }
}

RSSWidget::RSSWidget(QWidget *parent)
    : QWidget {parent}
    , m_ui {new Ui::RSSWidget}
{
    m_ui->setupUi(this);

    // Icons
    m_ui->actionCopyFeedURL->setIcon(UIThemeManager::instance()->getIcon("edit-copy"));
    m_ui->actionDelete->setIcon(UIThemeManager::instance()->getIcon("edit-delete"));
    m_ui->actionDownloadTorrent->setIcon(UIThemeManager::instance()->getIcon("download"));
    m_ui->actionMarkItemsRead->setIcon(UIThemeManager::instance()->getIcon("mail-mark-read"));
    m_ui->actionNewFolder->setIcon(UIThemeManager::instance()->getIcon("folder-new"));
    m_ui->actionNewSubscription->setIcon(UIThemeManager::instance()->getIcon("list-add"));
    m_ui->actionOpenNewsURL->setIcon(UIThemeManager::instance()->getIcon("application-x-mswinurl"));
    m_ui->actionRename->setIcon(UIThemeManager::instance()->getIcon("edit-rename"));
    m_ui->actionUpdate->setIcon(UIThemeManager::instance()->getIcon("view-refresh"));
    m_ui->actionUpdateAllFeeds->setIcon(UIThemeManager::instance()->getIcon("view-refresh"));
#ifndef Q_OS_MACOS
    m_ui->newFeedButton->setIcon(UIThemeManager::instance()->getIcon("list-add"));
    m_ui->markReadButton->setIcon(UIThemeManager::instance()->getIcon("mail-mark-read"));
    m_ui->updateAllButton->setIcon(UIThemeManager::instance()->getIcon("view-refresh"));
    m_ui->rssDownloaderBtn->setIcon(UIThemeManager::instance()->getIcon("download"));
#endif

    m_rssTreeView = new QTreeView {m_ui->splitterSide};
    m_ui->splitterSide->insertWidget(0, m_rssTreeView);
    auto *rssProxyModel = new RSSSortModel {m_rssTreeView};
    rssProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    rssProxyModel->setSourceModel(new RSSModel {m_rssTreeView});
    m_rssTreeView->setModel(rssProxyModel);
    connect(m_rssTreeView, &QAbstractItemView::doubleClicked, this, &RSSWidget::renameSelectedRSSItem);
    connect(m_rssTreeView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &RSSWidget::handleCurrentItemChanged);
    connect(m_rssTreeView, &QWidget::customContextMenuRequested, this, &RSSWidget::displayRSSListMenu);
    m_rssTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_rssTreeView->setDragEnabled(true);
    m_rssTreeView->setAcceptDrops(true);
    m_rssTreeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_rssTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    expandItems(Preferences::instance()->getRSSWidgetExpandedItems(), {});
    m_rssTreeView->setSortingEnabled(true);
    m_rssTreeView->sortByColumn(0, Qt::AscendingOrder);

    m_articleListView = new QListView {m_ui->splitterMain};
    m_ui->splitterMain->insertWidget(0, m_articleListView);
    m_rssFeedModel = new RSSFeedModel {nullptr, m_articleListView};
    auto *articleProxyModel = new RSSFeedSortModel {m_articleListView};
    articleProxyModel->setSourceModel(m_rssFeedModel);
    m_articleListView->setModel(articleProxyModel);
    connect(m_articleListView, &QListView::customContextMenuRequested, this, &RSSWidget::displayArticleListMenu);
    connect(m_articleListView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &RSSWidget::handleCurrentArticleChanged);
    connect(m_articleListView, &QListView::doubleClicked, this, &RSSWidget::downloadSelectedTorrents);
    m_articleListView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_articleListView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_rssTreeView->setCurrentIndex(m_rssTreeView->model()->index(0, 0));

    auto *editHotkey = new QShortcut {Qt::Key_F2, m_rssTreeView, nullptr, nullptr, Qt::WidgetShortcut};
    connect(editHotkey, &QShortcut::activated, this, &RSSWidget::renameSelectedRSSItem);
    auto *deleteHotkey = new QShortcut {QKeySequence::Delete, m_rssTreeView, nullptr, nullptr, Qt::WidgetShortcut};
    connect(deleteHotkey, &QShortcut::activated, this, &RSSWidget::deleteSelectedItems);

    // Feeds list actions
    connect(m_ui->actionDelete, &QAction::triggered, this, &RSSWidget::deleteSelectedItems);
    connect(m_ui->actionRename, &QAction::triggered, this, &RSSWidget::renameSelectedRSSItem);
    connect(m_ui->actionUpdate, &QAction::triggered, this, &RSSWidget::refreshSelectedItems);
    connect(m_ui->actionNewFolder, &QAction::triggered, this, &RSSWidget::askNewFolder);
    connect(m_ui->actionNewSubscription, &QAction::triggered, this, &RSSWidget::on_newFeedButton_clicked);
    connect(m_ui->actionUpdateAllFeeds, &QAction::triggered, this, &RSSWidget::refreshAllFeeds);
    connect(m_ui->updateAllButton, &QAbstractButton::clicked, this, &RSSWidget::refreshAllFeeds);
    connect(m_ui->actionCopyFeedURL, &QAction::triggered, this, &RSSWidget::copySelectedFeedsURL);
    connect(m_ui->actionMarkItemsRead, &QAction::triggered, this, &RSSWidget::on_markReadButton_clicked);

    // News list actions
    connect(m_ui->actionOpenNewsURL, &QAction::triggered, this, &RSSWidget::openSelectedArticlesUrls);
    connect(m_ui->actionDownloadTorrent, &QAction::triggered, this, &RSSWidget::downloadSelectedTorrents);

    // Restore sliders position
    restoreSlidersPosition();
    // Bind saveSliders slots
    connect(m_ui->splitterMain, &QSplitter::splitterMoved, this, &RSSWidget::saveSlidersPosition);
    connect(m_ui->splitterSide, &QSplitter::splitterMoved, this, &RSSWidget::saveSlidersPosition);

    if (RSS::Session::instance()->isProcessingEnabled())
        m_ui->labelWarn->hide();
    connect(RSS::Session::instance(), &RSS::Session::processingStateChanged
            , this, &RSSWidget::handleSessionProcessingStateChanged);
    connect(RSS::Session::instance()->rootFolder(), &RSS::Folder::unreadCountChanged
            , this, &RSSWidget::handleUnreadCountChanged);
}

RSSWidget::~RSSWidget()
{
    // we need it here to properly mark latest article
    // as read without having additional code
    m_articleListView->selectionModel()->setCurrentIndex({}, QItemSelectionModel::Clear);

    Preferences::instance()->setRSSWidgetExpandedItems(getExpandedItems({}));
    delete m_ui;
}

// display a right-click menu
void RSSWidget::displayRSSListMenu(const QPoint &pos)
{
    if (!m_rssTreeView->indexAt(pos).isValid())
        // No item under the mouse, clear selection
        m_rssTreeView->clearSelection();

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto selectedItems = m_rssTreeView->selectionModel()->selectedRows();
    if (!selectedItems.isEmpty()) {
        menu->addAction(m_ui->actionUpdate);
        menu->addAction(m_ui->actionMarkItemsRead);
        menu->addSeparator();

        if (selectedItems.size() == 1) {
            if (!isStickyItem(selectedItems.first())) {
                menu->addAction(m_ui->actionRename);
                menu->addAction(m_ui->actionDelete);
                menu->addSeparator();
                if (isFolder(selectedItems.first()))
                    menu->addAction(m_ui->actionNewFolder);
            }
        }
        else {
            menu->addAction(m_ui->actionDelete);
            menu->addSeparator();
        }

        menu->addAction(m_ui->actionNewSubscription);

        if (!isFolder(selectedItems.first())) {
            menu->addSeparator();
            menu->addAction(m_ui->actionCopyFeedURL);
        }
    }
    else {
        menu->addAction(m_ui->actionNewSubscription);
        menu->addAction(m_ui->actionNewFolder);
        menu->addSeparator();
        menu->addAction(m_ui->actionUpdateAllFeeds);
    }

    menu->popup(QCursor::pos());
}

void RSSWidget::displayArticleListMenu(const QPoint &)
{
    bool hasTorrent = false;
    bool hasLink = false;
    for (const QModelIndex &index : asConst(m_articleListView->selectionModel()->selectedRows())) {
        RSS::Article *article = getArticlePtr(index);
        Q_ASSERT(article);

        if (!article->torrentUrl().isEmpty())
            hasTorrent = true;
        if (!article->link().isEmpty())
            hasLink = true;
        if (hasTorrent && hasLink)
            break;
    }

    QMenu *myItemListMenu = new QMenu(this);
    myItemListMenu->setAttribute(Qt::WA_DeleteOnClose);

    if (hasTorrent)
        myItemListMenu->addAction(m_ui->actionDownloadTorrent);
    if (hasLink)
        myItemListMenu->addAction(m_ui->actionOpenNewsURL);

    if (!myItemListMenu->isEmpty())
        myItemListMenu->popup(QCursor::pos());
}

void RSSWidget::askNewFolder()
{
    bool ok;
    QString newName = AutoExpandableDialog::getText(
                this, tr("Please choose a folder name"), tr("Folder name:"), QLineEdit::Normal
                , tr("New folder"), &ok);
    if (!ok) return;

    newName = newName.trimmed();
    if (newName.isEmpty()) return;

    // Determine destination folder for new item
    QModelIndex destIndex;
    QModelIndexList selectedIndexes = m_rssTreeView->selectionModel()->selectedRows();
    if (!selectedIndexes.empty()) {
        destIndex = selectedIndexes.first();
        if (!isFolder(destIndex))
            destIndex = destIndex.parent();
    }
    // Consider the case where the user clicked on Unread item
    RSS::Folder *destFolder =
            !destIndex.isValid() ? RSS::Session::instance()->rootFolder()
                                   : static_cast<RSS::Folder *>(getItemPtr(destIndex));

    try {
        RSS::Session::instance()->addFolder(newName, destFolder);
        // Expand destination folder to display new feed
        if (destIndex.isValid() && !isStickyItem(destIndex))
            m_rssTreeView->expand(destIndex);
        // NOTE: Check the commented code below!
        // As new RSS items are added synchronously, we can do the following here.
//        m_rssTreeWidget->setCurrentItem(id);
    }
    catch (const RuntimeError &err) {
        QMessageBox::warning(this, "qBittorrent", err.message(), QMessageBox::Ok);
    }
}

// add a stream by a button
void RSSWidget::on_newFeedButton_clicked()
{
    // Ask for feed URL
    const QString clipText = qApp->clipboard()->text();
    const QString defaultURL = Net::DownloadManager::hasSupportedScheme(clipText) ? clipText : "http://";

    bool ok;
    QString newURL = AutoExpandableDialog::getText(
                this, tr("Please type a RSS feed URL"), tr("Feed URL:"), QLineEdit::Normal, defaultURL, &ok);
    if (!ok) return;

    newURL = newURL.trimmed();
    if (newURL.isEmpty()) return;

    // Determine destination folder for new item
    QModelIndex destIndex;
    QModelIndexList selectedIndexes = m_rssTreeView->selectionModel()->selectedRows();
    if (!selectedIndexes.empty()) {
        destIndex = selectedIndexes.first();
        if (!isFolder(destIndex))
            destIndex = destIndex.parent();
    }

    // Consider the case where the user clicked on Unread item
    RSS::Folder *destFolder =
            !destIndex.isValid() ? RSS::Session::instance()->rootFolder()
                                   : static_cast<RSS::Folder *>(getItemPtr(destIndex));

    try {
        RSS::Session::instance()->addFeed(newURL, "", destFolder);
        // Expand destination folder to display new feed
        if (destIndex.isValid() && !isStickyItem(destIndex))
            m_rssTreeView->expand(destIndex);
        // NOTE: Check the commented code below!
        // As new RSS items are added synchronously, we can do the following here.
//        m_rssTreeWidget->setCurrentItem(id);
    }
    catch (const RuntimeError &err) {
        QMessageBox::warning(this, "qBittorrent", err.message(), QMessageBox::Ok);
    }
}

void RSSWidget::deleteSelectedItems()
{
    const QModelIndexList selectedItems = m_rssTreeView->selectionModel()->selectedRows();
    if (selectedItems.isEmpty())
        return;
    if ((selectedItems.size() == 1) && (isStickyItem(selectedItems.first())))
        return;

    QMessageBox::StandardButton answer = QMessageBox::question(
                this, tr("Deletion confirmation"), tr("Are you sure you want to delete the selected RSS feeds?")
                , QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::No)
        return;

    for (const auto &index : selectedItems) {
        RSS::Item *rssItem = getItemPtr(index);
        try {
            RSS::Session::instance()->removeItem(rssItem);
        }
        catch (const RuntimeError &) {}
    }
}

void RSSWidget::refreshAllFeeds()
{
    RSS::Session::instance()->refresh();
}

void RSSWidget::downloadSelectedTorrents()
{
    for (const QModelIndex &index : asConst(m_articleListView->selectionModel()->selectedRows())) {
        RSS::Article *article = getArticlePtr(index);
        Q_ASSERT(article);

        // Mark as read
        article->markAsRead();

        if (!article->torrentUrl().isEmpty()) {
            if (AddNewTorrentDialog::isEnabled())
                AddNewTorrentDialog::show(article->torrentUrl(), window());
            else
                BitTorrent::Session::instance()->addTorrent(article->torrentUrl());
        }
    }
}

// open the url of the selected RSS articles in the Web browser
void RSSWidget::openSelectedArticlesUrls()
{
    for (const QModelIndex &index : asConst(m_articleListView->selectionModel()->selectedRows())) {
        RSS::Article *article = getArticlePtr(index);
        Q_ASSERT(article);

        // Mark as read
        article->markAsRead();

        if (!article->link().isEmpty())
            QDesktopServices::openUrl(QUrl(article->link()));
    }
}

void RSSWidget::renameSelectedRSSItem()
{
    const QModelIndexList selectedItems = m_rssTreeView->selectionModel()->selectedRows();
    if (selectedItems.size() != 1) return;

    if (isStickyItem(selectedItems.first()))
        return;

    RSS::Item *rssItem = getItemPtr(selectedItems.first());
    const QString name = rssItem->name();
    bool ok;
    do {
        QString newName = AutoExpandableDialog::getText(
                    this, tr("Please choose a new name for this RSS feed"), tr("New feed name:")
                    , QLineEdit::Normal, name, &ok);
        // Check if name is already taken
        if (!ok) return;

        try {
            RSS::Session::instance()->renameItem(rssItem, newName);
        }
        catch (const RuntimeError &err) {
            QMessageBox::warning(nullptr, tr("Rename failed"), err.message());
            ok = false;
        }
    } while (!ok);
}

void RSSWidget::refreshSelectedItems()
{
    for (const auto &index : asConst(m_rssTreeView->selectionModel()->selectedRows())) {
        RSS::Item *rssItem = getItemPtr(index);
        rssItem->refresh();
    }
}

void RSSWidget::copySelectedFeedsURL()
{
    QStringList URLs;
    for (const auto &index : asConst(m_rssTreeView->selectionModel()->selectedRows())) {
        RSS::Item *rssItem = getItemPtr(index);
        if (auto *feed = qobject_cast<RSS::Feed *>(rssItem))
            URLs << feed->url();
    }
    qApp->clipboard()->setText(URLs.join('\n'));
}

void RSSWidget::handleCurrentItemChanged(const QModelIndex &currentIndex)
{
    // we need it here to properly mark latest article
    // as read without having additional code
    m_articleListView->selectionModel()->setCurrentIndex({}, QItemSelectionModel::Clear);

    m_rssFeedModel->setRSSItem(getItemPtr(currentIndex));
}

void RSSWidget::on_markReadButton_clicked()
{
    for (const auto &index : asConst(m_rssTreeView->selectionModel()->selectedRows())) {
        RSS::Item *rssItem = getItemPtr(index);
        if (rssItem) {
            rssItem->markAsRead();
            if (rssItem == RSS::Session::instance()->rootFolder())
                break; // all items was read
        }
    }
}

// display a news
void RSSWidget::handleCurrentArticleChanged(const QModelIndex &currentIndex, const QModelIndex &previousIndex)
{
    m_ui->textBrowser->clear();

    if (previousIndex.isValid()) {
        RSS::Article *article = getArticlePtr(previousIndex);
        Q_ASSERT(article);
        article->markAsRead();
    }

    if (!currentIndex.isValid()) return;

    RSS::Article *article = getArticlePtr(currentIndex);
    Q_ASSERT(article);

    QString html =
        "<div style='border: 2px solid red; margin-left: 5px; margin-right: 5px; margin-bottom: 5px;'>"
        "<div style='background-color: #678db2; font-weight: bold; color: #fff;'>" + article->title() + "</div>";
    if (article->date().isValid())
        html += "<div style='background-color: #efefef;'><b>" + tr("Date: ") + "</b>" + article->date().toLocalTime().toString(Qt::SystemLocaleLongDate) + "</div>";
    if (!article->author().isEmpty())
        html += "<div style='background-color: #efefef;'><b>" + tr("Author: ") + "</b>" + article->author() + "</div>";
    html += "</div>"
            "<div style='margin-left: 5px; margin-right: 5px;'>";
    if (Qt::mightBeRichText(article->description())) {
        html += article->description();
    }
    else {
        QString description = article->description();
        QRegularExpression rx;
        // If description is plain text, replace BBCode tags with HTML and wrap everything in <pre></pre> so it looks nice
        rx.setPatternOptions(QRegularExpression::InvertedGreedinessOption
            | QRegularExpression::CaseInsensitiveOption);

        rx.setPattern("\\[img\\](.+)\\[/img\\]");
        description = description.replace(rx, "<img src=\"\\1\">");

        rx.setPattern("\\[url=(\")?(.+)\\1\\]");
        description = description.replace(rx, "<a href=\"\\2\">");
        description = description.replace("[/url]", "</a>", Qt::CaseInsensitive);

        rx.setPattern("\\[(/)?([bius])\\]");
        description = description.replace(rx, "<\\1\\2>");

        rx.setPattern("\\[color=(\")?(.+)\\1\\]");
        description = description.replace(rx, "<span style=\"color:\\2\">");
        description = description.replace("[/color]", "</span>", Qt::CaseInsensitive);

        rx.setPattern("\\[size=(\")?(.+)\\d\\1\\]");
        description = description.replace(rx, "<span style=\"font-size:\\2px\">");
        description = description.replace("[/size]", "</span>", Qt::CaseInsensitive);

        html += "<pre>" + description + "</pre>";
    }
    html += "</div>";
    m_ui->textBrowser->setHtml(html);
}

void RSSWidget::saveSlidersPosition()
{
    // Remember sliders positions
    Preferences *const pref = Preferences::instance();
    pref->setRssSideSplitterState(m_ui->splitterSide->saveState());
    pref->setRssMainSplitterState(m_ui->splitterMain->saveState());
}

void RSSWidget::restoreSlidersPosition()
{
    const Preferences *const pref = Preferences::instance();
    const QByteArray stateSide = pref->getRssSideSplitterState();
    if (!stateSide.isEmpty())
        m_ui->splitterSide->restoreState(stateSide);
    const QByteArray stateMain = pref->getRssMainSplitterState();
    if (!stateMain.isEmpty())
        m_ui->splitterMain->restoreState(stateMain);
}

void RSSWidget::updateRefreshInterval(uint val)
{
    RSS::Session::instance()->setRefreshInterval(val);
}

void RSSWidget::on_rssDownloaderBtn_clicked()
{
    auto *downloader = new AutomatedRssDownloader(this);
    downloader->setAttribute(Qt::WA_DeleteOnClose);
    downloader->open();
}

void RSSWidget::handleSessionProcessingStateChanged(bool enabled)
{
    m_ui->labelWarn->setVisible(!enabled);
}

void RSSWidget::handleUnreadCountChanged()
{
    emit unreadCountUpdated(RSS::Session::instance()->rootFolder()->unreadCount());
}

QStringList RSSWidget::getExpandedItems(const QModelIndex &index) const
{
    const auto *model = m_rssTreeView->model();
    QStringList result;
    for (int i = 0; i < model->rowCount(index); ++i) {
        const auto childIndex = model->index(i, 0, index);
        if (m_rssTreeView->isExpanded(childIndex))
            result.append(getItemPtr(childIndex)->path());
        result.append(getExpandedItems(childIndex));
    }

    return result;
}

void RSSWidget::expandItems(const QStringList &expandedItems, const QModelIndex &parent)
{
    const auto *model = m_rssTreeView->model();
    for (int i = 0; i < model->rowCount(parent); ++i) {
        const auto index = model->index(i, 0, parent);
        expandItems(expandedItems, index);

        if (expandedItems.contains(getItemPtr(index)->path()))
            m_rssTreeView->expand(index);
    }
}

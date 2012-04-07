/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPBOARDBROWSER_H
#define CLIPBOARDBROWSER_H

#include <QListView>
#include <QTimer>
#include <QClipboard>
#include "qeditor.h"
#include "configurationmanager.h"

class ClipboardModel;
class ItemDelegate;
class QMimeData;
class ClipboardItem;

class ClipboardBrowser : public QListView
{
    Q_OBJECT

    public:
        enum MenuAction {
            ActionToClipboard,
            ActionRemove,
            ActionEdit,
            ActionEditor,
            ActionAct,
            ActionCustom
        };

        explicit ClipboardBrowser(QWidget *parent = 0);
        ~ClipboardBrowser();
        void loadSettings();

        /**
         * Add new item to the browser.
         *
         * @param force if true ignore commands and add item even if it already
         *        is on top
         */
        bool add(const QString &txt, bool force = false);
        bool add(QMimeData *item, bool force = false);
        bool add(const ClipboardItem &item, bool force = false);

        void remove();
        void clear();

        bool select(uint item_hash);

        int length() const { return model()->rowCount(); }
        QString itemText(int i = -1) const;
        QString itemText(QModelIndex ind) const;
        const QMimeData *itemData(int i = -1) const;
        void sync(bool list_to_clipboard = true, QClipboard::Mode mode = QClipboard::Clipboard);
        QModelIndex index(int i) const {
            return model()->index(i,0);
        }
        void setCurrent(int row, bool cycle = false, bool selection = false);
        ClipboardItem *at(int row) const;

        // if items selected: return concatenation of selected items
        // else: return text of first item
        const QString selectedText() const;

        void updateClipboard();

        void redraw();

        void scrollTo(const QModelIndex &index,
                      QAbstractItemView::ScrollHint hint = EnsureVisible);

        void setAutoUpdate(bool update) { m_update = update; }

        QMenu *contextMenu() const {return m_menu;}

        /**
         * Set ID. Used to save items. If ID is empty saving is disabled.
         */
        void setID(const QString &id) { m_id = id; }
        const QString &getID() const { return m_id; }

    private:
        QString m_id;
        int m_maxitems;
        QString m_editor;
        QRegExp m_last_filter;
        ClipboardModel *m;
        ItemDelegate *d;
        QTimer timer_save;
        bool m_update;
        bool m_sizeHintChanged;

        QModelIndex m_scrollIndex;
        QAbstractItemView::ScrollHint m_scrollHint;

        QMenu *m_menu;
        ConfigurationManager::Commands commands;

        void createContextMenu();
        bool isFiltered(int row) const;

    protected:
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QContextMenuEvent *);
        void paintEvent(QPaintEvent *event);
        void dataChanged(const QModelIndex &a, const QModelIndex &b);

    signals:
        void requestSearch(const QString &txt);
        void requestActionDialog(const QString &text,
                                 const Command *cmd = NULL);
        void requestShow();
        void hideSearch();
        void escapePressed();
        void closeAllEditors();
        void changeClipboard(const ClipboardItem *item);

        void addToTab(QMimeData *data, const QString &tabName);

    private slots:
        void sizeHintChanged(const QModelIndex &index);

    public slots:
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); }
        void moveToClipboard(const QModelIndex &ind);
        void moveToClipboard(int i);
        void filterItems(const QString &str);
        void clearFilter() { filterItems( QString() ); }
        void itemModified(const QString &str);
        void closeEditor(QEditor *editor);
        bool openEditor(const QString &text);
        void addItems(const QStringList &items);

        void loadItems();
        void saveItems(int msec=0);
        void purgeItems();

        void contextMenuAction(QAction *act);
        void updateContextMenu();

        void newItem(const QString &text = QString());

        void checkClipboard(QClipboard::Mode mode, QMimeData *data);
};

#endif // CLIPBOARDBROWSER_H

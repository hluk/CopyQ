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

        ClipboardBrowser(const QString &id, QWidget *parent = 0);
        ~ClipboardBrowser();
        void loadSettings();
        bool add(const QString &txt, bool ignore_empty = true);
        bool add(QMimeData *item, bool ignore_empty = true);
        bool add(ClipboardItem *item);
        void remove();
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

        void runCallback() const;

        void updateClipboard();

        void setAutoUpdate(bool update) { m_update = update; }

        QMenu *contextMenu() const {return m_menu;}

    private:
        QString m_id;
        int m_maxitems;
        QString m_callback;
        QStringList m_callback_args;
        QString m_editor;
        QString m_last_filter;
        ClipboardModel *m;
        ItemDelegate *d;
        QTimer timer_save;
        bool m_update;
        QBasicTimer timer_preload;
        int m_to_preload;

        QMenu *m_menu;
        ConfigurationManager::Commands commands;

        void createContextMenu();

    protected:
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QContextMenuEvent *);
        void selectionChanged ( const QItemSelection & selected,
                                const QItemSelection & deselected );
        void timerEvent(QTimerEvent *event);

    signals:
        void requestSearch(const QString &txt);
        void requestActionDialog(ClipboardBrowser *c, int row, const ConfigurationManager::Command *cmd = NULL);
        void requestShow();
        void hideSearch();
        void escapePressed();
        void closeAllEditors();
        void changeClipboard(const ClipboardItem *item);

    private slots:
        void realDataChanged(const QModelIndex &a, const QModelIndex &b);

    public slots:
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); }
        void moveToClipboard(const QModelIndex &ind);
        void moveToClipboard(int i);
        void filterItems(const QString &str);
        void clearFilter() { filterItems( QString() ); }
        void itemModified(const QString &str);
        void closeEditor(QEditor *editor);
        void openEditor();
        void addItems(const QStringList &items);

        void loadItems();
        void saveItems(int msec=0);

        void preload(int msec=300);

        void contextMenuAction(QAction *act);
        void updateContextMenu();

        void newItem();

        void checkClipboard(QClipboard::Mode mode, QMimeData *data);
};

#endif // CLIPBOARDBROWSER_H

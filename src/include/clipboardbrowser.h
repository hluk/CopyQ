/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "command.h"

#include <QListView>
#include <QSharedPointer>

class ClipboardItem;
class ClipboardModel;
class ItemDelegate;
class QMimeData;
class QTimer;

struct ClipboardBrowserShared {
    ClipboardBrowserShared();

    void loadFromConfiguration();

    QString editor;
    int maxItems;
    QStringList formats;
    int maxImageWidth;
    int maxImageHeight;
    bool textWrap;
    QList<Command> commands;
    bool viMode;
    bool saveOnReturnKey;
    bool moveItemOnReturnKey;
};
typedef QSharedPointer<ClipboardBrowserShared> ClipboardBrowserSharedPtr;

/** List view of clipboard items. */
class ClipboardBrowser : public QListView
{
    Q_OBJECT

    public:
        /**
         * Disable ClipboardBrowser's automatic visual and clipboard updates.
         */
        class Lock {
            public:
                Lock(ClipboardBrowser *self);
                ~Lock();

            private:
                ClipboardBrowser *c;
                bool m_autoUpdate;
                bool m_updates;
        };

        explicit ClipboardBrowser(QWidget *parent = NULL,
                                  const ClipboardBrowserSharedPtr &sharedData = ClipboardBrowserSharedPtr());
        /** Close all external editors and save items if needed. */
        ~ClipboardBrowser();
        /** Load settings. */
        void loadSettings();

        /** Add new item to the browser. */
        bool add(
                const QString &txt, //!< Text of new item.
                bool force = false //!< If true ignore commands and duplicates.
                );
        /** Add new item to the browser. */
        bool add(
                QMimeData *item, //!< Data for new item.
                bool force = false, //!< If true ignore commands and duplicates.
                int row = 0 //!< Target row for the new item.
                );
        /** Add new item to the browser. */
        bool add(
                const ClipboardItem &item, //!< Item to copy.
                bool force = false, //!< If true ignore commands and duplicates.
                int row = 0 //!< Target row for the new item.
                );

        /** Remove all items. */
        void clear();

        /**
         * Select item with given @a hash and move it to clipboard.
         *
         * @return true only if item exists
         */
        bool select(
                uint item_hash //!< Hash of the item.
                );

        /** Sort selected items. */
        void sortItems(const QModelIndexList &indexes);

        /** Reverse order of selected items. */
        void reverseItems(const QModelIndexList &indexes);

        /** Number of items in list. */
        int length() const { return model()->rowCount(); }
        /** Text of item in given row or current row. */
        QString itemText(int i = -1) const;
        /** Text of item. */
        QString itemText(QModelIndex ind) const;
        /** Data of item in given row or current row. */
        const QMimeData *itemData(int i = -1) const;
        /** Index of item in given row. */
        QModelIndex index(int i) const { return model()->index(i,0); }
        /** Set current item. */
        void setCurrent(
                int row, //!< Row of the item.
                bool cycle = false, //!< If true @a row is relative number of rows from top.
                bool selection = false //!< Makes selection.
                );
        /** Return clipboard item at given row. */
        ClipboardItem *at(int row) const;

        /** Returns concatenation of selected items. */
        const QString selectedText() const;

        /** Update clipboard content according to first item in list. */
        void updateClipboard(int row = 0);

        /** Force redrawing the list. */
        void redraw();

        /** Toggle automatic clipboard update. */
        void setAutoUpdate(bool update) { m_update = update; }

        /** Return true if automatic clipboard update is on. */
        bool autoUpdate() { return m_update; }

        /** Return context menu. */
        QMenu *contextMenu() const {return m_menu;}

        /**
         * Set ID. Used to save items. If ID is empty saving is disabled.
         */
        void setID(const QString &id) { m_id = id; }
        const QString &getID() const { return m_id; }

        /**
         * Return true if editing is active.
         */
        bool editing();

        /**
         * Handle key for Vi mode.
         */
        bool handleViKey(QKeyEvent *event);

        /**
         * Wrap long text.
         */
        void setTextWrap(bool enabled);

        /**
         * Get data of selected item, NULL if none or multiple items selected.
         */
        const QMimeData *getSelectedItemData() const;

        /**
         * Add matching commands to menu.
         */
        void addCommandsToMenu(QMenu *menu, ///< Insert items before this action (if NULL append items).
                const QString &text,   ///< Text to match.
                const QMimeData *data  ///< MIME types to match.
                );

        /**
         * Set item data.
         */
        void setItemData(const QModelIndex &index, QMimeData *data);

    private:
        bool m_loaded;
        QString m_id;
        QRegExp m_lastFilter;
        bool m_update;
        ClipboardModel *m;
        ItemDelegate *d;
        QTimer *m_timerSave;

        QMenu *m_menu;

        ClipboardBrowserSharedPtr m_sharedData;

        void createContextMenu();
        bool isFiltered(int row) const;

        /**
         * Update scroll offset if size of an item is changed or new item is added.
         */
        void updateScrollOffset(const QModelIndex &index, int oldSize);

        /**
         * Connects signals and starts external editor.
         */
        bool startEditor(QObject *editor);

        /**
         * Select next/previous item and copy it to clipboard.
         */
        void copyItemToClipboard(int d);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QContextMenuEvent *);
        void paintEvent(QPaintEvent *event);
        void resizeEvent(QResizeEvent *event);
        void showEvent(QShowEvent *event);

    signals:
        /** Action dialog requested. */
        void requestActionDialog(const QMimeData &data);
        /** Action dialog requested. */
        void requestActionDialog(const QMimeData &data, const Command &cmd);
        /** Action dialog requested. */
        void requestActionDialog(const QMimeData &data, const Command &cmd, const QModelIndex &index);
        /** Show list request. */
        void requestShow(const ClipboardBrowser *self);
        /** Hide main window. */
        void requestHide();
        /** Request clipboard change. */
        void changeClipboard(const ClipboardItem *item);

        /** Add item to another tab (invoked by an automatic command). */
        void addToTab(const QMimeData *data, const QString &tabName);

        /** User begins or stops to edit an item in a tab. */
        void editingActive(bool active);

    protected slots:
        void commitData(QWidget *editor);

    private slots:
        void contextMenuAction();
        void updateContextMenu();

        void onDataChanged(const QModelIndex &a, const QModelIndex &b);

    public slots:
        /** Receive key event. */
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); }
        /** Move current item to clipboard. */
        void moveToClipboard();
        /** Move item to clipboard. */
        void moveToClipboard(const QModelIndex &ind);
        /** Move item to clipboard. */
        void moveToClipboard(int i);
        /** Show only items matching the pattern. */
        void filterItems(const QString &str);
        /** Show all items. */
        void clearFilter() { filterItems( QString() ); }
        /** Item modified in external editor. */
        void itemModified(const QByteArray &bytes, const QString &mime);
        /** Called if editor was closed. */
        void closeExternalEditor(QObject *editor);
        /** Open editor with text of all selected items or for single selected item. */
        bool openEditor();
        /** Open editor. */
        bool openEditor(const QByteArray &data, const QString &mime = QString("text/plain"),
                        const QString &editorCommand = QString());
        /** Open editor for an item. */
        bool openEditor(const QModelIndex &index);
        /** Add items. */
        void addItems(const QStringList &items);

        /** Show content of current item. */
        void showItemContent();

        /** Edit selected unhidden items. */
        void editSelected();
        /** Remove selected unhidden items. */
        void remove();

        void removeRow(int row);

        /** Open action dialog on selected items. */
        void action();

        /**
         * Load items from configuration.
         * @see setID, saveItems, purgeItems
         */
        void loadItems();
        /**
         * Save items to configuration.
         * @see setID, loadItems, purgeItems
         */
        void saveItems();
        /**
         * Save items to configuration after an interval.
         * Default interval is 30 seconds.
         */
        void delayedSaveItems(int msec = 30000);
        /**
         * Clear all items from configuration.
         * @see setID, loadItems, saveItems
         */
        void purgeItems();

        /**
         * Create and edit new item.
         */
        void editNew(
                const QString &text = QString() //!< Text of new item.
                );

        /**
         * Select next item and copy it to clipboard.
         */
        void copyNextItemToClipboard();

        /**
         * Select previous item and copy it to clipboard.
         */
        void copyPreviousItemToClipboard();
};

#endif // CLIPBOARDBROWSER_H

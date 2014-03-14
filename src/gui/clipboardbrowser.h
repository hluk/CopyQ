/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/command.h"
#include "gui/configtabshortcuts.h"
#include "item/clipboarditem.h"
#include "item/itemwidget.h"

#include <QListView>
#include <QPointer>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QVariantMap>

class ClipboardModel;
class ItemDelegate;
class ItemEditorWidget;
class QProgressBar;
class QPushButton;
class QTimer;

struct ClipboardBrowserShared {
    ClipboardBrowserShared();

    void loadFromConfiguration();

    QString editor;
    int maxItems;
    bool textWrap;
    QList<Command> commands;
    bool viMode;
    bool saveOnReturnKey;
    bool moveItemOnReturnKey;
    int minutesToExpire;
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
                Lock(ClipboardBrowser *self) : c(self) { c->lock(); }
                ~Lock() { if (!c.isNull()) c->unlock(); }

            private:
                QPointer<ClipboardBrowser> c;
        };

        explicit ClipboardBrowser(QWidget *parent = NULL,
                                  const ClipboardBrowserSharedPtr &sharedData = ClipboardBrowserSharedPtr());
        /** Close all external editors and save items if needed. */
        ~ClipboardBrowser();
        /** Load settings. */
        void loadSettings();

        /**
         * Select item with given @a hash and move it to clipboard.
         *
         * @return true only if item exists
         */
        bool select(uint itemHash, //!< Hash of the item.
                bool moveToTop = false, //!< Move existing item to top.
                bool moveToClipboard = true //!< Move existing item to clipboard
                );

        /** Sort selected items. */
        void sortItems(const QModelIndexList &indexes);

        /** Reverse order of selected items. */
        void reverseItems(const QModelIndexList &indexes);

        /** Text of item in given row or current row. */
        QString itemText(int i = -1) const;
        /** Text of item. */
        QString itemText(QModelIndex ind) const;
        /** Data of item in given row or current row. */
        QVariantMap itemData(int i = -1) const;
        /** Index of item in given row. */
        QModelIndex index(int i) const { return model()->index(i,0); }
        /** Return clipboard item at given row. */
        ClipboardItemPtr at(int row) const;

        /** Returns concatenation of selected items. */
        const QString selectedText() const;

        /** Update clipboard content according to first item in list. */
        void updateClipboard(int row = 0);

        /** Invalidate item cache. */
        void invalidateItemCache();

        /** Toggle automatic clipboard update. */
        void setAutoUpdate(bool update);

        /** Return true if automatic clipboard update is on. */
        bool autoUpdate() { return m_update; }

        /** Set context menu for item actions. */
        void setContextMenu(QMenu *menu);

        /**
         * Set ID. Used to save items. If ID is empty saving is disabled.
         */
        void setTabName(const QString &id);
        const QString &tabName() const { return m_tabName; }

        /**
         * Return true if editing is active.
         */
        bool editing() const;

        /**
         * Return true if items are loaded (loadItems() called or tabName() is empty).
         */
        bool isLoaded() const;

        /**
         * Close editor if unless user don't want to discard changed (show message box).
         *
         * @return true only if editor was closed
         */
        bool maybeCloseEditor();

        /**
         * Handle key for Vi mode.
         */
        bool handleViKey(QKeyEvent *event);

        /**
         * Get data of selected item, NULL if none or multiple items selected.
         */
        QVariantMap getSelectedItemData() const;

        /**
         * Add matching commands to menu.
         */
        void addCommandsToMenu(QMenu *menu, ///< Insert items before this action (if NULL append items).
                const QVariantMap &data  ///< MIME types to match.
                );

        /**
         * Override to disable default QAbstractItemView search.
         */
        void keyboardSearch(const QString &) {}

        /** Return true if user defined a selection and it shouldn't change programmatically. */
        bool hasUserSelection() const;

        QVariantMap copyIndexes(const QModelIndexList &indexes, bool serializeItems = true) const;

        /** Remove items and return row number of last removed item. */
        int removeIndexes(const QModelIndexList &indexes);

        /** Paste items. */
        void paste(const QVariantMap &data, int destinationRow);

        /** Render preview image with items. */
        QPixmap renderItemPreview(const QModelIndexList &indexes, int maxWidth, int maxHeight);

    public slots:
        /** Add new item to the browser. */
        bool add(
                const QString &txt, //!< Text of new item.
                int row = 0 //!< Target row for the new item (negative to append item).
                );

        /**
         * Add new item to the browser.
         * @a item is automatically deleted after it's no longer needed.
         */
        bool add(
                const QVariantMap &item, //!< Data for new item.
                int row = 0 //!< Target row for the new item (negative to append item).
                );

        /** Number of items in list. */
        int length() const { return model()->rowCount(); }

        /** Receive key event. */
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); }
        /** Move current item to clipboard. */
        void moveToClipboard();
        /** Move item to clipboard. */
        void moveToClipboard(const QModelIndex &ind);
        /** Move item to clipboard. */
        void moveToClipboard(int i);
        /** Show only items matching the regular expression. */
        void filterItems(const QRegExp &re);
        /** Show all items. */
        void clearFilter() { filterItems( QRegExp() ); }
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

        /** Edit notes for current item. */
        void editNotes();

        /** Set current item. */
        void setCurrent(
                int row, //!< Row of the item.
                bool cycle = false, //!< If true @a row is relative number of rows from top.
                bool selection = false //!< Makes selection.
                );

        /** Open action dialog on selected items. */
        void action();

        /**
         * Load items from configuration.
         * This function does nothing if model is disabled (e.g. loading failed previously).
         * @see setID, saveItems, purgeItems
         */
        void loadItems();

        /**
         * Load items from configuration even if model is disabled.
         * @see loadItems
         */
        void loadItemsAgain();

        /**
         * Save items to configuration.
         * @see setID, loadItems, purgeItems
         */
        bool saveItems();
        /**
         * Save items to configuration after an interval.
         */
        void delayedSaveItems();

        /**
         * Save items to configuration if needed.
         */
        void saveUnsavedItems();

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

        /**
         * Data of item in given row or current row.
         * If MIME type is "?" return list of available MIME types.
         */
        QByteArray itemData(int i, const QString &mime) const;

        /** Edit item in given @a row. */
        void editRow(int row);

    signals:
        /** Action dialog requested. */
        void requestActionDialog(const QVariantMap &data);
        /** Action dialog requested. */
        void requestActionDialog(const QVariantMap &data, const Command &cmd);
        /** Action dialog requested. */
        void requestActionDialog(const QVariantMap &data, const Command &cmd, const QModelIndex &index);
        /** Show list request. */
        void requestShow(const ClipboardBrowser *self);
        /** Hide main window. */
        void requestHide();
        /** Request clipboard change. */
        void changeClipboard(const QVariantMap &data);

        /** Context menu actions were updated. */
        void contextMenuUpdated();

        /** Add item to another tab (invoked by an automatic command). */
        void addToTab(const QVariantMap &data, const QString &tabName);

        /** Emitted on error. */
        void error(const QString &errorString);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void contextMenuEvent(QContextMenuEvent *);
        void resizeEvent(QResizeEvent *event);
        void showEvent(QShowEvent *event);
        void hideEvent(QHideEvent *event);
        void currentChanged(const QModelIndex &current, const QModelIndex &previous);
        void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
        void focusInEvent(QFocusEvent *event);

        void dragEnterEvent(QDragEnterEvent *event);
        void dragLeaveEvent(QDragLeaveEvent *event);
        void dragMoveEvent(QDragMoveEvent *event);
        void dropEvent(QDropEvent *event);

        void paintEvent(QPaintEvent *e);

        void mousePressEvent(QMouseEvent *event);
        void mouseReleaseEvent(QMouseEvent *event);
        void mouseMoveEvent(QMouseEvent *event);

    private slots:
        void contextMenuAction();
        void updateContextMenu();

        void onModelDataChanged();

        void onDataChanged(const QModelIndex &a, const QModelIndex &b);

        void onTabNameChanged(const QString &tabName);

        /** Delayed update. */
        void updateCurrentPage();

        /** Immediate update if possible. */
        void doUpdateCurrentPage();

        void expire();

        void onEditorDestroyed();

        void onEditorSave();

        void onEditorCancel();

        void onModelUnloaded();

        void filterItems();

    private:
        void createContextMenu();
        bool isFiltered(int row) const;

        /**
         * Hide row if filtered out, otherwise show.
         * @return true only if hidden
         */
        bool hideFiltered(int row);

        /**
         * Connects signals and starts external editor.
         */
        bool startEditor(QObject *editor);

        /**
         * Select next/previous item and copy it to clipboard.
         */
        void copyItemToClipboard(int d);

        /**
         * Preload items in given range (relative to current scroll offset).
         */
        void preload(int minY, int maxY);

        void setEditorWidget(ItemEditorWidget *widget);

        void editItem(const QModelIndex &index, bool editNotes = false);

        void updateEditorGeometry();

        void initSingleShotTimer(QTimer *timer, int milliseconds, const char *slot = NULL);

        void restartExpiring();

        void stopExpiring();

        void updateCurrentItem();

        void initActions();

        void clearActions();

        QAction *createAction(Actions::Id id, const char *slot);

        /**
         * Get index near given @a point.
         * If space between items is at the @a point, return next item.
         */
        QModelIndex indexNear(int offset) const;

        void updateSearchProgress();

        int getDropRow(const QPoint &position);

        void connectModelAndDelegate();

        void disconnectModel();

        void updateItemMaximumSize();

        void lock();
        void unlock();

        ItemLoaderInterfacePtr m_itemLoader;
        QString m_tabName;
        int m_lastFiltered;
        bool m_update;
        ClipboardModel *m;
        ItemDelegate *d;
        QTimer *m_timerSave;
        QTimer *m_timerScroll;
        QTimer *m_timerUpdate;
        QTimer *m_timerFilter;
        QTimer *m_timerExpire;

        QPointer<QMenu> m_menu;

        bool m_invalidateCache;
        bool m_expire;

        ItemEditorWidget *m_editor;

        ClipboardBrowserSharedPtr m_sharedData;

        QPushButton *m_loadButton;
        QProgressBar *m_searchProgress;

        int m_dragPosition;
        QPoint m_dragStartPosition;

        int m_spinLock;
        bool m_updateLock;
        QScopedPointer<class ScrollSaver> m_scrollSaver;
};

bool canExecuteCommand(const Command &command, const QVariantMap &data, const QString &sourceTabName);

#endif // CLIPBOARDBROWSER_H

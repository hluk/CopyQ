/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "item/clipboardmodel.h"
#include "item/itemdelegate.h"
#include "item/itemwidget.h"

#include <QListView>
#include <QPointer>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QVariantMap>

class ItemEditorWidget;
class ItemFactory;
class QProgressBar;
class QPushButton;
class Theme;

enum SelectAction {
    NoSelectAction,
    MoveToTop = 1 << 1,
    MoveToClipboard = 1 << 2
};
Q_DECLARE_FLAGS(SelectActions, SelectAction)
Q_DECLARE_OPERATORS_FOR_FLAGS(SelectActions)

struct ClipboardBrowserShared {
    explicit ClipboardBrowserShared(ItemFactory *itemFactory);

    void loadFromConfiguration();

    QString editor;
    int maxItems;
    bool textWrap;
    bool viMode;
    bool saveOnReturnKey;
    bool moveItemOnReturnKey;
    int minutesToExpire;

    ItemFactory *itemFactory;
};
typedef QSharedPointer<ClipboardBrowserShared> ClipboardBrowserSharedPtr;

QVariantMap itemData(const QModelIndex &index);

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

        explicit ClipboardBrowser(const ClipboardBrowserSharedPtr &sharedData, QWidget *parent = NULL);
        /** Close all external editors and save items if needed. */
        ~ClipboardBrowser();
        /** Load settings. */
        void loadSettings();

        /**
         * Select item with given @a hash and move it to clipboard.
         *
         * @return true only if item exists
         */
        bool select(uint itemHash, SelectActions selectActions);

        /** Sort selected items. */
        void sortItems(const QModelIndexList &indexes);

        /** Reverse order of selected items. */
        void reverseItems(const QModelIndexList &indexes);

        /** Index of item in given row. */
        QModelIndex index(int i) const { return model()->index(i,0); }

        /** Returns concatenation of selected items. */
        const QString selectedText() const;

        /** Invalidate item cache. */
        void invalidateItemCache();

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
         * Get data of selected item, NULL if none or multiple items selected.
         */
        QVariantMap getSelectedItemData() const;

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

        /**
         * Add item and remove duplicates.
         */
        void addUnique(const QVariantMap &data);

        /** Number of items in list. */
        int length() const { return model()->rowCount(); }

        /** Receive key event. */
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); }
        /** Move item to clipboard. */
        void moveToClipboard(const QModelIndex &ind);
        /** Show only items matching the regular expression. */
        void filterItems(const QRegExp &re);
        /** Show all items. */
        void clearFilter() { filterItems( QRegExp() ); }
        /** Open editor. */
        bool openEditor(const QByteArray &textData, bool changeClipboard = false);
        /** Open editor for an item. */
        bool openEditor(const QModelIndex &index);
        /** Add items. */
        void addItems(const QStringList &items);

        void removeRow(int row);

        /** Set current item. */
        void setCurrent(
                int row, //!< Row of the item.
                bool cycle = false, //!< If true @a row is relative number of rows from top.
                bool keepSelection = false, //!< Keeps current selection.
                bool setCurrentOnly = false //! Sets item as current.
                );

        /**
         * Load items from configuration even if model is disabled.
         * @see loadItems
         */
        void loadItemsAgain();

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
                const QString &text = QString(), //!< Text of new item.
                bool changeClipboard = false //!< Change clipboard if item is modified.
                );

        /** Edit item in given @a row. */
        void editRow(int row);

        void otherItemLoader(bool next);

        void move(int key);

        /// Decorate browser and items with @a theme.
        void decorate(const Theme &theme);

    public slots:
        /**
         * Load items from configuration.
         * This function does nothing if model is disabled (e.g. loading failed previously).
         * @see setID, saveItems, purgeItems
         */
        void loadItems();

        /**
         * Save items to configuration.
         * @see setID, loadItems, purgeItems
         */
        bool saveItems();

        /** Move current item to clipboard. */
        void moveToClipboard();

        /** Edit selected unhidden items. */
        void editSelected();

        /** Edit notes for current item. */
        void editNotes();

        /** Open editor with text of all selected items or for single selected item. */
        bool openEditor();

        /** Remove selected unhidden items. */
        void remove();

        /** Show content of current item. */
        void showItemContent();

        /** Item modified in external editor. */
        void itemModified(const QByteArray &bytes, const QString &mime);
        /** Called if editor was closed. */
        void closeExternalEditor(QObject *editor);

    signals:
        /** Action dialog requested. */
        void requestActionDialog(const QVariantMap &data);
        /** Action dialog requested. */
        void requestActionDialog(const QVariantMap &data, const Command &cmd, const QModelIndex &index);
        /** Show list request. */
        void requestShow(const ClipboardBrowser *self);
        /** Request clipboard change. */
        void changeClipboard(const QVariantMap &data);

        /** Emitted on error. */
        void error(const QString &errorString);

        void itemCountChanged(const QString &tabName, int count);

        void showContextMenu(const QPoint &position);

        void updateContextMenu();

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
        void onModelDataChanged();

        void onDataChanged(const QModelIndex &a, const QModelIndex &b);

        void onItemCountChanged();

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

        void onEditorNeedsChangeClipboard();

        void onEditorNeedsChangeClipboard(const QByteArray &bytes, const QString &mime);

        void filterItems();

    private:
        /**
         * Save items to configuration after an interval.
         */
        void delayedSaveItems();

        bool isFiltered(int row) const;

        /**
         * Hide row if filtered out, otherwise show.
         * @return true only if hidden
         */
        bool hideFiltered(int row);

        /**
         * Connects signals and starts external editor.
         */
        bool startEditor(QObject *editor, bool changeClipboard = false);

        /**
         * Select next/previous item and copy it to clipboard.
         */
        void copyItemToClipboard(int d);

        /**
         * Preload items in given range (relative to current scroll offset).
         */
        void preload(int minY, int maxY);

        void setEditorWidget(ItemEditorWidget *widget, bool changeClipboard = false);

        void editItem(const QModelIndex &index, bool editNotes = false, bool changeClipboard = false);

        void updateEditorGeometry();

        bool canExpire();

        void restartExpiring();

        void stopExpiring();

        void updateCurrentItem();

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

        void refilterItems();

        ItemLoaderInterface *m_itemLoader;
        QString m_tabName;
        int m_lastFiltered;
        ClipboardModel m;
        ItemDelegate d;
        QTimer m_timerSave;
        QTimer m_timerScroll;
        QTimer m_timerUpdate;
        QTimer m_timerFilter;
        QTimer m_timerExpire;

        bool m_invalidateCache;
        bool m_expireAfterEditing;

        ItemEditorWidget *m_editor;
        bool m_editClipboard;

        ClipboardBrowserSharedPtr m_sharedData;

        QPushButton *m_loadButton;
        QProgressBar *m_searchProgress;

        int m_dragTargetRow;
        QPoint m_dragStartPosition;

        int m_spinLock;
        QScopedPointer<class ScrollSaver> m_scrollSaver;
};

#endif // CLIPBOARDBROWSER_H

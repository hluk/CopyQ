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

#ifndef ITEMWIDGET_H
#define ITEMWIDGET_H

#include "tests/testinterface.h"

#include <QRegExp>
#include <QStringList>
#include <QtPlugin>
#include <QVariantMap>

class QAbstractItemModel;
class QTextEdit;
class QFile;
class QFont;
class QModelIndex;
class QPalette;
class QRegExp;
class QWidget;
struct Command;

#define COPYQ_PLUGIN_ITEM_LOADER_ID "org.CopyQ.ItemPlugin.ItemLoader/1.0"

#if QT_VERSION < 0x050000
#   define Q_PLUGIN_METADATA(x)
#else
#   if defined(Q_EXPORT_PLUGIN)
#       undef Q_EXPORT_PLUGIN
#       undef Q_EXPORT_PLUGIN2
#   endif
#   define Q_EXPORT_PLUGIN(x)
#   define Q_EXPORT_PLUGIN2(x,y)
#endif

/**
 * Handles item in list.
 */
class ItemWidget
{
public:
    explicit ItemWidget(QWidget *widget);

    virtual ~ItemWidget() {}

    /**
     * Set search and selections highlight color and font.
     */
    void setHighlight(const QRegExp &re, const QFont &highlightFont,
                      const QPalette &highlightPalette);

    /**
     * Return widget to render.
     */
    QWidget *widget() const { return m_widget; }

    /**
     * Create editor widget with given @a parent.
     * Editor widget can have signals:
     *   1. save(), to save data,
     *   2. cancel(), if hasChanges() is true a dialog will pop-up asking user whether to save
     *      changes, otherwise editor closes,
     *   3. invalidate(), to close editor immediately.
     */
    virtual QWidget *createEditor(QWidget *parent) const;

    /**
     * Set data for editor widget.
     */
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;

    /**
     * Set data from editor widget to model.
     */
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

    /**
     * Return true if data in editor were changed.
     */
    virtual bool hasChanges(QWidget *editor) const;

    /**
     * Create external editor for @a index.
     *
     * Default implementation returns NULL.
     *
     * @param index  index for which the editor is opened
     *
     * @return Editor object -- see documentation for public signals and slots of ItemEditor class --
     *         NULL so default text editor is opened.
     */
    virtual QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const;

    /**
     * Size of widget needs to be updated (because maximum size chaged).
     */
    virtual void updateSize(const QSize &maximumSize, int idealSize);

    /**
     * Called if widget is set or unset as current.
     */
    virtual void setCurrent(bool current);

protected:
    /**
     * Highlight matching text with given font and color.
     * Default implementation does nothing.
     */
    virtual void highlight(const QRegExp &, const QFont &, const QPalette &) {}

    /**
     * Filter mouse events for QTextEdit widgets.
     *
     * With Shift modifier pressed (item should be selected first), text in QTextEdit widget
     * can be selected.
     *
     * Without Shift modifier pressed, mouse press selects item and mouse move with left button
     * pressed drags item.
     *
     * Use QWidget::installEventFilter() on QTextEdit::viewport() and call this method from
     * overridden QWidget::eventFilter().
     */
    bool filterMouseEvents(QTextEdit *edit, QEvent *event);

private:
    QRegExp m_re;
    QWidget *m_widget;
};

class ItemLoaderInterface
{
public:
    ItemLoaderInterface() {}

    virtual ~ItemLoaderInterface() {}

    /**
     * Return priority.
     *
     * Use this loader before all others with lower priority.
     */
    virtual int priority() const { return 0; }

    /**
     * Create ItemWidget instance from index data.
     *
     * @return NULL if index hasn't appropriate data
     */
    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent) const;

    /**
     * Simple ID of plugin (e.g. part of plugin file name).
     */
    virtual QString id() const = 0;

    /**
     * Return descriptive name.
     */
    virtual QString name() const = 0;

    /**
     * Return author's name.
     */
    virtual QString author() const = 0;

    /**
     * Return plugin description.
     */
    virtual QString description() const = 0;

    /**
     * Return icon representing the plugin.
     *
     * Return value can be QIcon or UInt (to render a character from icon font). Default is no icon.
     */
    virtual QVariant icon() const { return QVariant(); }

    /**
     * Provide formats to save (possibly configurable).
     *
     * The values are stored into user QSettings, under group with name same as value of id().
     */
    virtual QStringList formatsToSave() const { return QStringList(); }

    /**
     * Save and return configuration values to save from current settings widget.
     */
    virtual QVariantMap applySettings() { return QVariantMap(); }

    /**
     * Load stored configuration values.
     */
    virtual void loadSettings(const QVariantMap &) {}

    /**
     * Create settings widget.
     */
    virtual QWidget *createSettingsWidget(QWidget *) { return NULL; }

    /**
     * @return true only if items can be loaded
     */
    virtual bool canLoadItems(QFile *file) const;

    /**
     * @return true only if items can be saved
     */
    virtual bool canSaveItems(const QAbstractItemModel &model) const;

    /**
     * Load items.
     * @return true only if items were saved by this plugin (or just not to load them any further)
     */
    virtual bool loadItems(QAbstractItemModel *model, QFile *file);

    /**
     * Save items.
     * @return true only if items were saved
     */
    virtual bool saveItems(const QAbstractItemModel &model, QFile *file);

    /**
     * Initialize tab (tab was not yet saved or loaded).
     * @return true only if successful
     */
    virtual bool initializeTab(QAbstractItemModel *model);

    /**
     * Uninitialize tab (tab will be handled by different plugin).
     */
    virtual void uninitializeTab(QAbstractItemModel *model);

    /**
     * Allow to transform item widget (wrap around a new widget).
     * By default returns NULL not to wrap the widget.
     * New ItemWidget must take care of deleting the old one!
     */
    virtual ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index);

    /**
     * Called before items are deleted by user.
     * @return true if items can be removed, false to cancel the removal
     */
    virtual bool canRemoveItems(const QList<QModelIndex> &indexList);

    /**
     * Called before items are moved out of list (i.e. deleted) by user.
     * @return true if items can be moved, false to cancel the removal
     */
    virtual bool canMoveItems(const QList<QModelIndex> &);

    /**
     * Called when items are being deleted by user.
     */
    virtual void itemsRemovedByUser(const QList<QModelIndex> &indexList);

    /**
     * Return copy of items data.
     */
    virtual QVariantMap copyItem(const QAbstractItemModel &model, const QVariantMap &itemData);

    /**
     * Return true if regular expression matches items content.
     * Returns false by default.
     */
    virtual bool matches(const QModelIndex &index, const QRegExp &re) const;

    /**
     * Return object with tests.
     *
     * All private slots are executed (see QtTest documentation).
     *
     * Property "CopyQ_test_settings" contains configuration for GUI server and
     * if will be passed to ItemLoaderInterface::loadSettings() for this plugin.
     */
    virtual QObject *tests(const TestInterfacePtr &test) const;

    /**
     * Return QObject instance with signals (by default null pointer).
     * Returned QObject must have signal error(QString) for signaling errors.
     */
    virtual const QObject *signaler() const;

    /**
     * Return script to run before client scripts.
     */
    virtual QString script() const;

    /**
     * Adds commands from scripts for command dialog.
     */
    virtual QList<Command> commands() const;
};

Q_DECLARE_INTERFACE(ItemLoaderInterface, COPYQ_PLUGIN_ITEM_LOADER_ID)

#endif // ITEMWIDGET_H

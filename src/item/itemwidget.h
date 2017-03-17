/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include <memory>

class QAbstractItemModel;
class QTextEdit;
class QIODevice;
class QFont;
class QModelIndex;
class QPalette;
class QRegExp;
class QWidget;
struct Command;

class ItemLoaderInterface;
using ItemLoaderPtr = std::shared_ptr<ItemLoaderInterface>;

class ItemSaverInterface;
using ItemSaverPtr = std::shared_ptr<ItemSaverInterface>;

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

    virtual ~ItemWidget() = default;

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
     * Default implementation returns nullptr.
     *
     * @param index  index for which the editor is opened
     *
     * @return Editor object -- see documentation for public signals and slots of ItemEditor class --
     *         nullptr so default text editor is opened.
     */
    virtual QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const;

    /**
     * Size of widget needs to be updated (because maximum size chaged).
     */
    virtual void updateSize(const QSize &maximumSize, int idealWidth);

    /**
     * Mark item as tagged/untagged.
     *
     * Used to hide unimportant data when notes or tags are present.
     */
    virtual void setTagged(bool) {}

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

class ItemScriptable : public QObject
{
    Q_OBJECT
public:
    explicit ItemScriptable(QObject *parent) : QObject(parent) {}

    QObject *scriptable() const { return m_scriptable; }
    void setScriptable(QObject *scriptable) { m_scriptable = scriptable; }
    virtual void start() {}

protected:
    QVariant call(const QString &method, const QVariantList &arguments = QVariantList());
    QVariant eval(const QString &script);
    QVariantList currentArguments();

private:
    QObject *m_scriptable = nullptr;
};

class ItemSaverInterface
{
public:
    virtual ~ItemSaverInterface() = default;

    /**
     * Save items.
     * @return true only if items were saved
     */
    virtual bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file);

    /**
     * Called before items are deleted by user.
     * @return true if items can be removed, false to cancel the removal
     */
    virtual bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error);

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
};

class ItemLoaderInterface
{
public:
    ItemLoaderInterface() = default;

    virtual ~ItemLoaderInterface() = default;

    /**
     * Return priority.
     *
     * Use this loader before all others with lower priority.
     */
    virtual int priority() const { return 0; }

    /**
     * Create ItemWidget instance from index data.
     *
     * @return nullptr if index hasn't appropriate data
     */
    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent, bool preview) const;

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
    virtual QWidget *createSettingsWidget(QWidget *) { return nullptr; }

    /**
     * @return true only if items can be loaded
     */
    virtual bool canLoadItems(QIODevice *file) const;

    /**
     * @return true only if items can be saved
     */
    virtual bool canSaveItems(const QString &tabName) const;

    /**
     * Load items.
     * @return true only if items were saved by this plugin (or just not to load them any further)
     */
    virtual ItemSaverPtr loadItems(
            const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems);

    /**
     * Initialize tab (tab was not yet saved or loaded).
     * @return true only if successful
     */
    virtual ItemSaverPtr initializeTab(const QString &tabName, QAbstractItemModel *model, int maxItems);

    /**
     * Allow to transform item widget (wrap around a new widget).
     * By default returns nullptr not to wrap the widget.
     * New ItemWidget must take care of deleting the old one!
     */
    virtual ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index);

    /**
     * Transform loader.
     */
    virtual ItemSaverPtr transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model);

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
     *
     * Returned QObject can have signal error(QString) for signaling errors
     * and addCommands(QList<Command>) to open and add commands to Command dialog.
     */
    virtual const QObject *signaler() const;

    /**
     * Return scriptable object that can be used in scripts.
     *
     * Object will be available as "plugins.<PLUGIN_ID>".
     */
    virtual ItemScriptable *scriptableObject(QObject *parent);

    /**
     * Adds commands from scripts for command dialog.
     */
    virtual QList<Command> commands() const;
};

Q_DECLARE_INTERFACE(ItemLoaderInterface, COPYQ_PLUGIN_ITEM_LOADER_ID)

#endif // ITEMWIDGET_H

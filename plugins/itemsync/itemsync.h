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

#ifndef ITEMSYNC_H
#define ITEMSYNC_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QScopedPointer>
#include <QWidget>

namespace Ui {
class ItemSyncSettings;
}

class FileWatcher;
class QTextEdit;
struct FileFormat;

class ItemSync : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    ItemSync(const QString &label, const QString &icon, ItemWidget *childItem = NULL);

protected:
    virtual void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette);

    virtual QWidget *createEditor(QWidget *parent) const;

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;

    virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const;

    virtual bool hasChanges(QWidget *editor) const;

    virtual QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const;

    virtual void updateSize(const QSize &maximumSize, int idealWidth);

    virtual bool eventFilter(QObject *, QEvent *event);

private:
    QTextEdit *m_label;
    QWidget *m_icon;
    QScopedPointer<ItemWidget> m_childItem;
};

/**
 * Synchronizes selected tab with destination path.
 *
 * For all tabs that have user-set synchronization directory, loads up to maximum number of items
 * from files (tries to use the same files every time tab is loaded).
 *
 * Items contains base name of assigned files (MIME is 'application/x-copyq-itemsync-basename').
 * E.g. files 'example.txt', 'example.html' and 'example_notes.txt' belong to single item with
 * base name 'example' containing text, HTML and notes.
 *
 * If item data are changed it is saved to appropriate files and vice versa.
 *
 * If files is in synchronization directory but is of unknown type, hidden, unreadable or its
 * file name starts with dot (this is hidden file on UNIX so lets make it same everywhere) it
 * won't be added to the list.
 *
 * Unknown file types can be defined in settings so such files are loaded.
 *
 * Item data with unknown MIME type is serialized in '<BASE NAME>_copyq.dat' file.
 */
class ItemSyncLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemSyncLoader();
    ~ItemSyncLoader();

    virtual QString id() const { return "itemsync"; }
    virtual QString name() const { return tr("Synchronize"); }
    virtual QString author() const { return QString(); }
    virtual QString description() const { return tr("Synchronize items and notes with a directory on disk."); }
    virtual QVariant icon() const { return QVariant(IconUploadAlt); }

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings);

    virtual QWidget *createSettingsWidget(QWidget *parent);

    virtual bool canLoadItems(QFile *file) const;

    virtual bool canSaveItems(const QAbstractItemModel &model) const;

    virtual bool loadItems(QAbstractItemModel *model, QFile *file);

    virtual bool saveItems(const QAbstractItemModel &model, QFile *file);

    virtual bool initializeTab(QAbstractItemModel *model);

    virtual void uninitializeTab(QAbstractItemModel *model);

    virtual ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index);

    virtual bool canRemoveItems(const QList<QModelIndex> &indexList);

    virtual bool canMoveItems(const QList<QModelIndex> &indexList);

    virtual void itemsRemovedByUser(const QList<QModelIndex> &indexList);

    virtual QVariantMap copyItem(const QAbstractItemModel &model, const QVariantMap &itemData);

    virtual bool matches(const QModelIndex &index, const QRegExp &re) const;

    virtual QObject *tests(const TestInterfacePtr &test) const;

    virtual const QObject *signaler() const { return this; }

signals:
    void error(const QString &);

private slots:
    void removeWatcher(QObject *watcher);
    void removeModel();
    void onBrowseButtonClicked();

private:
    QString tabPath(const QAbstractItemModel &model) const;

    bool loadItems(QAbstractItemModel *model, const QStringList &files);

    QScopedPointer<Ui::ItemSyncSettings> ui;
    QVariantMap m_settings;
    QMap<const QObject*, FileWatcher*> m_watchers;
    QMap<QString, QString> m_tabPaths;
    QList<FileFormat> m_formatSettings;
};

#endif // ITEMSYNC_H

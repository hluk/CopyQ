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

#ifndef ITEMSYNC_H
#define ITEMSYNC_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QWidget>

namespace Ui {
class ItemSyncSettings;
}

class FileWatcher;
class QLabel;
class QTextEdit;

class ItemSync : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    ItemSync(const QString &label, bool replaceChildItem, ItemWidget *childItem = NULL);

protected:
    virtual void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette);

    virtual void updateSize();

    virtual void mousePressEvent(QMouseEvent *e);

    virtual void mouseDoubleClickEvent(QMouseEvent *e);

    virtual void contextMenuEvent(QContextMenuEvent *e);

    virtual void mouseReleaseEvent(QMouseEvent *e);

    virtual bool eventFilter(QObject *obj, QEvent *event);

private slots:
    void onSelectionChanged();

private:
    QTextEdit *m_label;
    QLabel *m_icon;
    QScopedPointer<ItemWidget> m_childItem;
};

/**
 * Synchronizes selected tab with destination path.
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
    virtual QString description() const { return tr("Synchronize items with a directory on disk."); }
    virtual QVariant icon() const { return QVariant(IconUploadAlt); }

    virtual QVariantMap applySettings();

    virtual void loadSettings(const QVariantMap &settings);

    virtual QWidget *createSettingsWidget(QWidget *parent);

    virtual bool loadItems(const QString &tabName, QAbstractItemModel *model, QFile *file);

    virtual bool saveItems(const QString &tabName, const QAbstractItemModel &model, QFile *file);

    virtual bool createTab(const QString &tabName, QAbstractItemModel *model, QFile *file);

    virtual ItemWidget *transform(ItemWidget *itemWidget, const QModelIndex &index);

private slots:
    void removeWatcher(QObject *watcher);
    void removeModel();
    void onBrowseButtonClicked();

private:
    bool shouldSyncTab(const QString &tabName) const;
    QString tabPath(const QString &tabName) const;
    FileWatcher *createWatcher(QAbstractItemModel *model, const QStringList &paths);

    Ui::ItemSyncSettings *ui;
    QVariantMap m_settings;
    QMap<const QObject*, FileWatcher*> m_watchers;
    QMap<QString, QString> m_tabPaths;
};

#endif // ITEMSYNC_H

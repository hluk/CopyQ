/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include "item/itemwidgetwrapper.h"

#include <QWidget>

#include <memory>

namespace Ui {
class ItemSyncSettings;
}

class QTextEdit;

class FileWatcher;
struct FileFormat;

using ItemSyncTabPaths = QMap<QString, QString>;

class ItemSync final : public QWidget, public ItemWidgetWrapper
{
    Q_OBJECT

public:
    ItemSync(const QString &label, const QString &icon, ItemWidget *childItem = nullptr);

protected:
    void highlight(const QRegularExpression &re, const QFont &highlightFont,
                           const QPalette &highlightPalette) override;

    void updateSize(QSize maximumSize, int idealWidth) override;

    bool eventFilter(QObject *, QEvent *event) override;

private:
    QTextEdit *m_label;
    QWidget *m_icon;
};

class ItemSyncSaver final : public QObject, public ItemSaverInterface
{
    Q_OBJECT

public:
    explicit ItemSyncSaver(const QString &tabPath);

    ItemSyncSaver(
            QAbstractItemModel *model,
            const QString &tabPath,
            const QString &path,
            const QStringList &files,
            int maxItems,
            const QList<FileFormat> &formatSettings);

    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override;

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override;

    void itemsRemovedByUser(const QList<QModelIndex> &indexList) override;

    QVariantMap copyItem(const QAbstractItemModel &model, const QVariantMap &itemData) override;

    void setFocus(bool focus) override;

private:
    QString m_tabPath;
    FileWatcher *m_watcher;
};

class ItemSyncScriptable final : public ItemScriptable
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap tabPaths READ getTabPaths CONSTANT)
    Q_PROPERTY(QString mimeBaseName READ getMimeBaseName CONSTANT)

public:
    explicit ItemSyncScriptable(const QVariantMap &tabPaths)
        : m_tabPaths(tabPaths)
    {
    }

    QVariantMap getTabPaths() const { return m_tabPaths; }

    QString getMimeBaseName() const;

public slots:
    QString selectedTabPath();

private:
    QVariantMap m_tabPaths;
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
class ItemSyncLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemSyncLoader();
    ~ItemSyncLoader();

    QString id() const override { return "itemsync"; }
    QString name() const override { return tr("Synchronize"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Synchronize items and notes with a directory on disk."); }
    QVariant icon() const override { return QVariant(IconUpload); }

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    bool canLoadItems(QIODevice *file) const override;

    bool canSaveItems(const QString &tabName) const override;

    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems) override;

    ItemSaverPtr initializeTab(const QString &tabName, QAbstractItemModel *model, int maxItems) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    bool matches(const QModelIndex &index, const QRegularExpression &re) const override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

signals:
    void error(const QString &);

private slots:
    void onBrowseButtonClicked();

private:
    ItemSaverPtr loadItems(const QString &tabName, QAbstractItemModel *model, const QStringList &files, int maxItems);

    std::unique_ptr<Ui::ItemSyncSettings> ui;
    QVariantMap m_settings;
    ItemSyncTabPaths m_tabPaths;
    QList<FileFormat> m_formatSettings;
};

#endif // ITEMSYNC_H

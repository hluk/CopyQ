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

#ifndef ITEMPINNED_H
#define ITEMPINNED_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QWidget>

#include <memory>

class ItemPinned : public QWidget, public ItemWidget
{
    Q_OBJECT

public:
    explicit ItemPinned(ItemWidget *childItem);

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                           const QPalette &highlightPalette) override;

    QWidget *createEditor(QWidget *parent) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const override;

    bool hasChanges(QWidget *editor) const override;

    QObject *createExternalEditor(const QModelIndex &index, QWidget *parent) const override;

    void updateSize(QSize maximumSize, int idealWidth) override;

private:
    QWidget *m_border;
    std::unique_ptr<ItemWidget> m_childItem;
};

class ItemPinnedScriptable : public ItemScriptable
{
    Q_OBJECT
public slots:
    bool isPinned();

    void pin();
    void unpin();

    void pinData();
    void unpinData();
};

class ItemPinnedSaver : public QObject, public ItemSaverInterface
{
    Q_OBJECT

public:
    ItemPinnedSaver(QAbstractItemModel *model, QVariantMap &settings, const ItemSaverPtr &saver);

    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override;

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override;

    bool canMoveItems(const QList<QModelIndex> &indexList) override;

    void itemsRemovedByUser(const QList<QModelIndex> &indexList) override;

    QVariantMap copyItem(const QAbstractItemModel &model, const QVariantMap &itemData) override;

private slots:
    void onRowsInserted(const QModelIndex &parent, int start, int end);
    void onRowsRemoved(const QModelIndex &parent, int start, int end);
    void onRowsMoved(const QModelIndex &, int start, int end, const QModelIndex &, int destinationRow);
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    void moveRow(int from, int to);
    void updateLastPinned(int from, int to);

    QPointer<QAbstractItemModel> m_model;
    QVariantMap m_settings;
    ItemSaverPtr m_saver;

    // Last pinned row in list (improves performace of updates).
    int m_lastPinned = -1;
};

class ItemPinnedLoader : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemPinnedLoader();
    ~ItemPinnedLoader();

    QString id() const override { return "itempinned"; }
    QString name() const override { return tr("Pinned Items"); }
    QString author() const override { return QString(); }
    QString description() const override {
        return tr("Pin items to lock them in current row and avoid deletion (unless unpinned).");
    }
    QVariant icon() const override { return QVariant(IconThumbtack); }

    QStringList formatsToSave() const override;

    QVariantMap applySettings() override;

    void loadSettings(const QVariantMap &settings) override { m_settings = settings; }

    QWidget *createSettingsWidget(QWidget *parent) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    ItemSaverPtr transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

private:
    QVariantMap m_settings;
    ItemLoaderPtr m_transformedLoader;
};

#endif // ITEMPINNED_H

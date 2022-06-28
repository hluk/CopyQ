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

#ifndef ITEMPINNED_H
#define ITEMPINNED_H

#include "gui/icons.h"
#include "item/itemwidgetwrapper.h"
#include "item/itemsaverwrapper.h"

#include <QWidget>

class ItemPinned final : public QWidget, public ItemWidgetWrapper
{
    Q_OBJECT

public:
    explicit ItemPinned(ItemWidget *childItem);

protected:
    void paintEvent(QPaintEvent *paintEvent) override;

    void updateSize(QSize maximumSize, int idealWidth) override;
};

class ItemPinnedScriptable final : public ItemScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString mimePinned READ getMimePinned CONSTANT)

public slots:
    bool isPinned();

    void pin();
    void unpin();

    void pinData();
    void unpinData();

    QString getMimePinned() const;
};

class ItemPinnedSaver final : public QObject, public ItemSaverWrapper
{
    Q_OBJECT

public:
    ItemPinnedSaver(QAbstractItemModel *model, const ItemSaverPtr &saver);

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override;

    bool canDropItem(const QModelIndex &index) override;

    bool canMoveItems(const QList<QModelIndex> &indexList) override;

private:
    void onRowsInserted(const QModelIndex &parent, int start, int end);
    void onRowsRemoved(const QModelIndex &parent, int start, int end);
    void onRowsMoved(const QModelIndex &, int start, int end, const QModelIndex &, int destinationRow);
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    void moveRow(int from, int to);
    void updateLastPinned(int from, int to);

    QPointer<QAbstractItemModel> m_model;

    // Last pinned row in list (improves performace of updates).
    int m_lastPinned = -1;
};

class ItemPinnedLoader final : public QObject, public ItemLoaderInterface
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
        return tr(
            "<p>Pin items to lock them in current row and avoid deletion (unless unpinned).</p>"
            "<p>Provides shortcuts and scripting functionality.</p>"
        );
    }
    QVariant icon() const override { return QVariant(IconThumbtack); }

    QStringList formatsToSave() const override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    ItemSaverPtr transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model) override;

    QObject *tests(const TestInterfacePtr &test) const override;

    const QObject *signaler() const override { return this; }

    ItemScriptable *scriptableObject() override;

    QVector<Command> commands() const override;

private:
    ItemLoaderPtr m_transformedLoader;
};

#endif // ITEMPINNED_H

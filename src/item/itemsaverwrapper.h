#pragma once

#include "item/itemwidget.h"

class ItemSaverWrapper : public ItemSaverInterface
{
public:
    explicit ItemSaverWrapper(const ItemSaverPtr &saver);

    bool saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file) override;

    bool canRemoveItems(const QList<QModelIndex> &indexList, QString *error) override;

    bool canDropItem(const QModelIndex &index) override;

    bool canMoveItems(const QList<QModelIndex> &indexList) override;

    void itemsRemovedByUser(const QList<QPersistentModelIndex> &indexList) override;

    bool copyItem(
        const QAbstractItemModel &model,
        const QVariantMap &itemData,
        QVariantMap *copiedItemData,
        QString *error) override;

    void setFocus(bool focus) override;

protected:
    ItemSaverInterface *wrapped() const { return m_saver.get(); }

private:
    ItemSaverPtr m_saver;
};

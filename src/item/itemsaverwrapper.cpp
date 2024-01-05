#include "itemsaverwrapper.h"

#include <QVariantMap>

ItemSaverWrapper::ItemSaverWrapper(const ItemSaverPtr &saver)
    : m_saver(saver)
{
}

bool ItemSaverWrapper::saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file)
{
    return m_saver->saveItems(tabName, model, file);
}

bool ItemSaverWrapper::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    return m_saver->canRemoveItems(indexList, error);
}

bool ItemSaverWrapper::canDropItem(const QModelIndex &index)
{
    return m_saver->canDropItem(index);
}

bool ItemSaverWrapper::canMoveItems(const QList<QModelIndex> &indexList)
{
    return m_saver->canMoveItems(indexList);
}

void ItemSaverWrapper::itemsRemovedByUser(const QList<QPersistentModelIndex> &indexList)
{
    return m_saver->itemsRemovedByUser(indexList);
}

QVariantMap ItemSaverWrapper::copyItem(const QAbstractItemModel &model, const QVariantMap &itemData)
{
    return m_saver->copyItem(model, itemData);
}

void ItemSaverWrapper::setFocus(bool focus)
{
    return m_saver->setFocus(focus);
}

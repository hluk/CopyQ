#pragma once

#include <QtContainerFwd>

class QAbstractItemModel;
class QModelIndex;
class QPersistentModelIndex;
typedef QList<QModelIndex> QModelIndexList;

QList<QPersistentModelIndex> toPersistentModelIndexList(const QList<QModelIndex> &indexes);

int dropIndexes(const QModelIndexList &indexes, QAbstractItemModel *model);

int dropIndexes(QList<QPersistentModelIndex> &indexes, QAbstractItemModel *model);

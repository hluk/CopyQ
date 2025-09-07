#pragma once

#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>

class QAbstractItemModel;

QList<QPersistentModelIndex> toPersistentModelIndexList(const QList<QModelIndex> &indexes);

int dropIndexes(const QModelIndexList &indexes, QAbstractItemModel *model);

int dropIndexes(QList<QPersistentModelIndex> &indexes, QAbstractItemModel *model);

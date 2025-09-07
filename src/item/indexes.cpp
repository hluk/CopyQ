// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexes.h"

#include <QAbstractItemModel>

QList<QPersistentModelIndex> toPersistentModelIndexList(const QList<QModelIndex> &indexes)
{
    QList<QPersistentModelIndex> result;
    result.reserve( indexes.size() );

    for (const auto &index : indexes) {
        if ( index.isValid() )
            result.append(index);
    }

    return result;
}

int dropIndexes(const QModelIndexList &indexes, QAbstractItemModel *model)
{
    auto toRemove = toPersistentModelIndexList(indexes);
    return dropIndexes(toRemove, model);
}

int dropIndexes(QList<QPersistentModelIndex> &indexes, QAbstractItemModel *model)
{
    const int first = indexes.value(0).row();

    std::sort( std::begin(indexes), std::end(indexes) );

    // Remove ranges of rows instead of a single rows.
    for (auto it1 = std::begin(indexes); it1 != std::end(indexes); ) {
        if ( it1->isValid() ) {
            const auto firstRow = it1->row();
            auto rowCount = 0;

            for ( ++it1, ++rowCount; it1 != std::end(indexes)
                  && it1->isValid()
                  && it1->row() == firstRow + rowCount; ++it1, ++rowCount ) {}

            model->removeRows(firstRow, rowCount);
        } else {
            ++it1;
        }
    }

    return first;
}

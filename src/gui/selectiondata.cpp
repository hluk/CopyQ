// SPDX-License-Identifier: GPL-3.0-or-later
#include "selectiondata.h"

#include "common/mimetypes.h"
#include "gui/clipboardbrowser.h"

void addSelectionData(
        QVariantMap *result,
        const QList<QPersistentModelIndex> &selectedIndexes)
{
    result->insert(mimeSelectedItems, QVariant::fromValue(selectedIndexes));
}

void addSelectionData(
        QVariantMap *result,
        const QModelIndexList &selectedIndexes)
{
    QList<QPersistentModelIndex> selected;
    selected.reserve(selectedIndexes.size());
    for (const auto &index : selectedIndexes)
        selected.append(index);
    std::sort(selected.begin(), selected.end());
    addSelectionData(result, selected);
}

/// Adds information about current tab and selection if command is triggered by user.
QVariantMap selectionData(
        const ClipboardBrowser &c,
        const QModelIndex &currentIndex,
        const QModelIndexList &selectedIndexes)
{
    auto result = c.copyIndexes(selectedIndexes);

    result.insert(mimeCurrentTab, c.tabName());

    if ( currentIndex.isValid() ) {
        const QPersistentModelIndex current = currentIndex;
        result.insert(mimeCurrentItem, QVariant::fromValue(current));
    }

    if ( !selectedIndexes.isEmpty() ) {
        addSelectionData(&result, selectedIndexes);
    }

    return result;
}

QVariantMap selectionData(const ClipboardBrowser &c)
{
    const QModelIndexList selectedIndexes = c.selectionModel()->selectedIndexes();
    const auto current = c.selectionModel()->currentIndex();
    return selectionData(c, current, selectedIndexes);
}

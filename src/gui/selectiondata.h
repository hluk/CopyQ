// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QModelIndexList>
#include <QVariantMap>
#include <QtContainerFwd>

class ClipboardBrowser;

void addSelectionData(
        QVariantMap *result,
        const QList<QPersistentModelIndex> &selectedIndexes);

void addSelectionData(
        QVariantMap *result,
        const QModelIndexList &selectedIndexes);

/// Adds information about current tab and selection if command is triggered by user.
QVariantMap selectionData(
        const ClipboardBrowser &c,
        const QModelIndex &currentIndex,
        const QModelIndexList &selectedIndexes);

QVariantMap selectionData(const ClipboardBrowser &c);

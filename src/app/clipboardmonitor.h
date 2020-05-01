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

#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include "common/common.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformclipboard.h"

#include <QVariantMap>

enum class ClipboardOwnership {
    Foreign,
    Own,
    Hidden,
};

class ClipboardMonitor final : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(const QStringList &formats);

signals:
    void clipboardChanged(const QVariantMap &data, ClipboardOwnership ownership);
    void clipboardUnchanged(const QVariantMap &data);
    void synchronizeSelection(ClipboardMode sourceMode, const QString &text, uint targetTextHash);

private:
    void onClipboardChanged(ClipboardMode mode);

    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;

    PlatformClipboardPtr m_clipboard;
    QStringList m_formats;

    QString m_clipboardTab;
    bool m_storeClipboard;

#ifdef HAS_MOUSE_SELECTIONS
    bool m_storeSelection;
    bool m_runSelection;
    bool m_clipboardToSelection;
    bool m_selectionToClipboard;
#endif
};

#endif // CLIPBOARDMONITOR_H

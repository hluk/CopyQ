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

#include "shortcuts.h"

#include <QKeySequence>
#include <QObject>
#include <QString>

namespace {

int indexOfKeyHint(const QString &name)
{
    bool amp = false;
    int i = 0;

    for (const auto &c : name) {
        if (c == '&')
            amp = !amp;
        else if (amp)
            return i - 1;
        ++i;
    }

    return -1;
}

} // namespace

QString shortcutToRemove()
{
#ifdef Q_OS_MAC
    return QObject::tr("Backspace", "Key to remove item or MIME on OS X");
#else
    return QObject::tr("Delete", "Key to remove item or MIME");
#endif
}

QString portableShortcutText(const QKeySequence &shortcut)
{
    // WORKAROUND: Qt has convert some keys to upper case which
    //             breaks some shortcuts on some keyboard layouts.
    return shortcut.toString(QKeySequence::PortableText).toLower();
}

QString toPortableShortcutText(const QString &shortcutNativeText)
{
    return portableShortcutText(
                QKeySequence(shortcutNativeText, QKeySequence::NativeText));
}

bool hasKeyHint(const QString &name)
{
    return indexOfKeyHint(name) != -1;
}

QString &removeKeyHint(QString *name)
{
    const int i = indexOfKeyHint(*name);
    return i == -1 ? *name : name->remove(i, 1);
}

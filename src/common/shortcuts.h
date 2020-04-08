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

#ifndef SHORTCUTS_H
#define SHORTCUTS_H

class QString;
class QKeySequence;

/**
 * Shortcut to remove items, formats etc.
 */
QString shortcutToRemove();

QString portableShortcutText(const QKeySequence &shortcut);

QString toPortableShortcutText(const QString &shortcutNativeText);

/// Returns true only if UI name contains key hint (unescaped '&').
bool hasKeyHint(const QString &name);

/// Removes key hint (first unescaped '&') from UI name.
QString &removeKeyHint(QString *name);

#endif // SHORTCUTS_H

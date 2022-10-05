// SPDX-License-Identifier: GPL-3.0-or-later

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

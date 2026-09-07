// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcuts.h"

#include <QKeySequence>
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
    // This is serialized/parsing syntax, not a user-facing label.
#ifdef Q_OS_MAC
    return QStringLiteral("Backspace");
#else
    return QStringLiteral("Delete");
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

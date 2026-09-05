// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests.h"

#include "common/shortcuts.h"
#include "gui/menuitems.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QSet>
#include <QString>
#include <QTest>
#include <QTranslator>

namespace {

class InvalidShortcutTranslator final : public QTranslator
{
public:
    QString translate(
        const char *context, const char *sourceText,
        const char *disambiguation = nullptr, int n = -1) const override
    {
        Q_UNUSED(disambiguation)
        Q_UNUSED(n)

        if (qstrcmp(context, "QObject") != 0)
            return {};

        const QString source = QString::fromLatin1(sourceText);
        if (source.contains('+')
            || source == QLatin1String("Escape")
            || source == QLatin1String("Delete")
            || source == QLatin1String("Backspace")
            || source == QLatin1String("Left")
            || source == QLatin1String("Right")
            || (source.size() >= 2 && source.at(0) == QLatin1Char('F')))
        {
            return QStringLiteral("Not a valid shortcut");
        }

        return {};
    }
};

} // namespace

void CoreTests::applicationShortcutDefaultsIgnoreUiTranslations()
{
    InvalidShortcutTranslator translator;
    QVERIFY(QCoreApplication::installTranslator(&translator));
    const auto items = menuItems();
    QVERIFY(QCoreApplication::removeTranslator(&translator));

    const QSet<Actions::Id> intentionallyEmpty{
        Actions::Editor_Font,
        Actions::Editor_Strikethrough,
        Actions::Editor_Foreground,
        Actions::Editor_Background,
        Actions::Editor_EraseStyle,
        Actions::Item_MoveToClipboard,
    };

    for (int i = 0; i < Actions::Count; ++i) {
        const auto id = static_cast<Actions::Id>(i);
        const auto &item = items[id];
        if (intentionallyEmpty.contains(id)) {
            QVERIFY2(item.defaultShortcut.isEmpty(), qPrintable(item.settingsKey));
        } else {
            QVERIFY2(!item.defaultShortcut.isEmpty(), qPrintable(item.settingsKey));
        }
    }

    QCOMPARE(
        items[Actions::Edit_SortSelectedItems].defaultShortcut,
        QKeySequence(QStringLiteral("Ctrl+Shift+S"), QKeySequence::PortableText));
    QCOMPARE(
        items[Actions::Editor_Cancel].defaultShortcut,
        QKeySequence(QStringLiteral("Escape"), QKeySequence::PortableText));
    QCOMPARE(
        items[Actions::Item_MoveUp].defaultShortcut,
        QKeySequence(QStringLiteral("Ctrl+Up"), QKeySequence::PortableText));
    QCOMPARE(
        items[Actions::Tabs_NextTab].defaultShortcut,
        QKeySequence(QStringLiteral("Right"), QKeySequence::PortableText));
    QVERIFY(!QKeySequence(shortcutToRemove(), QKeySequence::PortableText).isEmpty());
}

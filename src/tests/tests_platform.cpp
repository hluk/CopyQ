// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_platform.h"

#include "platform/platformcommon.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QTest>

#include <memory>

namespace {

class StubPlatformWindow final : public PlatformWindow {
public:
    explicit StubPlatformWindow(QString title)
        : m_title(std::move(title))
    {
    }

    QString getTitle() override { return m_title; }
    void raise() override {}
    bool pasteFromClipboard() override { return true; }
    bool copyToClipboard() override { return true; }
    bool matchesWidget(const QWidget *) const override { return false; }

private:
    QString m_title;
};

PlatformWindowPtr makeWindow(const QString &title)
{
    return std::make_shared<StubPlatformWindow>(title);
}

} // namespace

PlatformTests::PlatformTests(const TestInterfacePtr &, QObject *parent)
    : QObject(parent)
{
    setProperty("CopyQ_test_id", QStringLiteral("platform"));
}

/**
 * The automatic paste target must be the window the user came from.
 *
 * On Windows the main window can be activated by the OS (taskbar/tray icon
 * click), so the previously active window is no longer in the foreground when
 * MainWindow::updateFocusWindows() samples it. In that case the current
 * window is one of CopyQ's own windows (or none at all) and the paste target
 * must fall back to the last active window of another application.
 */
void PlatformTests::pasteTargetWindowResolution()
{
    const auto foreign = makeWindow(QStringLiteral("Other App"));
    const auto foreign2 = makeWindow(QStringLiteral("Another App"));
    const auto own = makeWindow(QStringLiteral("CopyQ"));

    // Current foreground window belongs to another application - use it.
    QCOMPARE(
        resolvePasteTargetWindow(foreign, nullptr, false).get(),
        foreign.get() );
    QCOMPARE(
        resolvePasteTargetWindow(foreign, foreign2, false).get(),
        foreign.get() );

    // Current window is one of our own windows (e.g. main window activated by
    // clicking its taskbar icon) - fall back to the last active foreign window.
    QCOMPARE(
        resolvePasteTargetWindow(own, foreign, true).get(),
        foreign.get() );

    // No current window but a foreign window is known - use the foreign one.
    QCOMPARE(
        resolvePasteTargetWindow(nullptr, foreign, false).get(),
        foreign.get() );

    // Own window and no known foreign window - keep the current window
    // (previous behavior, the caller decides what to do with it).
    QCOMPARE(
        resolvePasteTargetWindow(own, nullptr, true).get(),
        own.get() );

    // Nothing known at all - no paste target.
    QVERIFY( !resolvePasteTargetWindow(nullptr, nullptr, false) );
    QVERIFY( !resolvePasteTargetWindow(nullptr, nullptr, true) );
}

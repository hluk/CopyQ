// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_clipboardmonitor.h"

#include "app/clipboardmonitor.h"
#include "common/mimetypes.h"
#include "platform/platformclipboard.h"

#include <QMimeData>
#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <utility>

namespace {

class FakeClipboard final : public PlatformClipboard {
public:
    ClipboardReadResult readData(ClipboardMode, const QStringList &) const override
    {
        ++m_readCount;
        if (m_results.isEmpty()) {
            ClipboardReadResult result;
            result.isComplete = true;
            return result;
        }

        return m_results.takeFirst();
    }

    void appendResult(ClipboardReadResult result)
    {
        m_results.append(std::move(result));
    }

    void notifyChanged()
    {
        emitConnectionChanged(ClipboardMode::Clipboard);
    }

    int readCount() const { return m_readCount; }

    void setData(ClipboardMode, const QVariantMap &) override {}
    void setRawData(ClipboardMode, QMimeData *) override {}
    const QMimeData *mimeData(ClipboardMode) const override { return nullptr; }
    bool isSelectionSupported() const override { return false; }
    bool isHidden(const QMimeData &) const override { return false; }
    void setClipboardOwner(const QString &) override {}

protected:
    void startMonitoringBackend(const QStringList &, ClipboardModeMask) override {}
    void stopMonitoringBackend() override {}

private:
    mutable QList<ClipboardReadResult> m_results;
    mutable int m_readCount = 0;
};

ClipboardReadResult failedRead()
{
    return {};
}

ClipboardReadResult textRead(
        const QByteArray &text, const QByteArray &owner = QByteArray())
{
    ClipboardReadResult result;
    result.data.insert(mimeText, text);
    result.availableFormats.append(mimeText);
    if (!owner.isEmpty()) {
        result.data.insert(mimeOwner, owner);
        result.availableFormats.append(mimeOwner);
    }
    result.isComplete = true;
    return result;
}

} // namespace

ClipboardMonitorTests::ClipboardMonitorTests(const TestInterfacePtr &, QObject *parent)
    : QObject(parent)
{
    setProperty("CopyQ_test_id", QStringLiteral("clipboardmonitor"));
}

void ClipboardMonitorTests::incompleteReadRetriesBeforePublishing()
{
    auto clipboard = std::make_shared<FakeClipboard>();
    clipboard->appendResult(failedRead());
    clipboard->appendResult(textRead("screenshot"));

    ClipboardMonitor monitor({mimeText}, clipboard);
    QSignalSpy changed(&monitor, &ClipboardMonitor::clipboardChanged);
    monitor.startMonitoring();

    clipboard->notifyChanged();
    QCOMPARE(changed.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 1000);
    QCOMPARE(clipboard->readCount(), 2);
    QCOMPARE(changed.first().first().toMap().value(mimeText).toByteArray(), QByteArray("screenshot"));
}

void ClipboardMonitorTests::newerChangeCancelsOlderRetry()
{
    auto clipboard = std::make_shared<FakeClipboard>();
    clipboard->appendResult(failedRead());
    clipboard->appendResult(textRead("newer"));

    ClipboardMonitor monitor({mimeText}, clipboard);
    QSignalSpy changed(&monitor, &ClipboardMonitor::clipboardChanged);
    monitor.startMonitoring();

    clipboard->notifyChanged();
    clipboard->notifyChanged();

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 1000);
    QTest::qWait(100);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(clipboard->readCount(), 2);
    QCOMPARE(changed.first().first().toMap().value(mimeText).toByteArray(), QByteArray("newer"));
}

void ClipboardMonitorTests::ownerMetadataChangeDoesNotRepublishContent()
{
    auto clipboard = std::make_shared<FakeClipboard>();
    clipboard->appendResult(textRead("activated item", "COPYQ OWNER"));
    clipboard->appendResult(textRead("activated item"));

    ClipboardMonitor monitor({mimeText}, clipboard);
    QSignalSpy ownChanged(&monitor, &ClipboardMonitor::ownClipboardChanged);
    QSignalSpy externalChanged(&monitor, &ClipboardMonitor::clipboardChanged);
    QSignalSpy unchanged(&monitor, &ClipboardMonitor::clipboardUnchanged);
    monitor.startMonitoring();

    clipboard->notifyChanged();
    QCOMPARE(ownChanged.count(), 1);
    QCOMPARE(externalChanged.count(), 0);

    // Simulate a paste target republishing the same bytes without CopyQ's
    // private owner MIME. This must not become a new external copy event.
    clipboard->notifyChanged();
    QCOMPARE(ownChanged.count(), 1);
    QCOMPARE(externalChanged.count(), 0);
    QCOMPARE(unchanged.count(), 1);
}

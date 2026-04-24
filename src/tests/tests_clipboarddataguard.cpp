// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_clipboarddataguard.h"

#include "common/clipboarddataguard.h"

#include <QMimeData>
#include <QSet>
#include <QTest>
#include <QUrl>

#include <new>

ClipboardDataGuardTests::ClipboardDataGuardTests(const TestInterfacePtr &, QObject *parent)
    : QObject(parent)
{
    setProperty("CopyQ_test_id", QStringLiteral("clipboarddataguard"));
}

class ThrowingMimeData : public QMimeData {
public:
    void setThrowFor(const QString &mime) { m_throwMimes.insert(mime); }
protected:
    QVariant retrieveData(const QString &mime, QMetaType type) const override {
        if (m_throwMimes.contains(mime))
            throw std::bad_alloc();
        return QMimeData::retrieveData(mime, type);
    }
private:
    QSet<QString> m_throwMimes;
};

void ClipboardDataGuardTests::badAllocData()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setData("text/plain", "hello");
    md.setThrowFor("text/plain");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.data("text/plain").isEmpty());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

void ClipboardDataGuardTests::badAllocUrls()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setThrowFor("text/uri-list");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.urls().isEmpty());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

void ClipboardDataGuardTests::badAllocText()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setThrowFor("text/plain");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.text().isEmpty());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

void ClipboardDataGuardTests::badAllocImageData()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setThrowFor("application/x-qt-image");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.getImageData().isNull());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

void ClipboardDataGuardTests::badAllocGetUtf8Data()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setThrowFor("text/html");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.getUtf8Data("text/html").isEmpty());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

void ClipboardDataGuardTests::normalDataStillWorks()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:-1");
    ThrowingMimeData md;
    md.setData("application/octet-stream", "binary");
    md.setThrowFor("text/plain");  // only text/plain throws
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("application/octet-stream"), QByteArray("binary"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
}

static void bumpConfigGeneration()
{
    const int gen = qApp->property("CopyQ_config_generation").toInt();
    qApp->setProperty("CopyQ_config_generation", gen + 1);
}

void ClipboardDataGuardTests::overflowTreatedAsNoLimit()
{
    // 9999999999G overflows qint64 (9999999999 * 1073741824 > INT64_MAX).
    // parseByteSize must return -1 (no limit), so data passes through.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:9999999999G");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "overflow-test");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("overflow-test"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

void ClipboardDataGuardTests::emptyRulesIgnored()
{
    // Empty entries from leading/trailing/consecutive semicolons are skipped.
    // The valid rule text/plain:5 must still apply.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ";text/plain:5;;");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "hi");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("hi"));

    // Oversized for the 5-byte limit.
    QMimeData md2;
    md2.setData("text/plain", "this is way over five bytes");
    ClipboardDataGuard guard2(&md2);
    QVERIFY(guard2.data("text/plain").isEmpty());
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}


void ClipboardDataGuardTests::negativeValueMeansNoLimit()
{
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", "text/plain:-1");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "anything");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("anything"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

void ClipboardDataGuardTests::invalidRulesFallBackToDefault()
{
    // .*:abc is not a valid size — falls back to built-in default (.*:100M).
    // Small data still passes.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", ".*:abc");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "small");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("small"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

void ClipboardDataGuardTests::suffixParsing()
{
    // K suffix: 1K = 1024 bytes.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", "text/plain:1K");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "short");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("short"));

    // G suffix: 1G = huge limit, small data passes.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", "text/plain:1G");
    bumpConfigGeneration();
    QMimeData md2;
    md2.setData("text/plain", "gtest");
    ClipboardDataGuard guard2(&md2);
    QCOMPARE(guard2.data("text/plain"), QByteArray("gtest"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

void ClipboardDataGuardTests::firstMatchWins()
{
    // First rule blocks text/plain, second allows all.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", "text/plain:0;.*:100M");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "blocked");
    md.setData("application/octet-stream", "allowed");
    ClipboardDataGuard guard(&md);
    QVERIFY(guard.data("text/plain").isEmpty());
    QCOMPARE(guard.data("application/octet-stream"), QByteArray("allowed"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

void ClipboardDataGuardTests::emptySizeMeansNoLimit()
{
    // "text/plain:" (empty size after colon) means no limit.
    qputenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT", "text/plain:");
    bumpConfigGeneration();
    QMimeData md;
    md.setData("text/plain", "any data at all");
    ClipboardDataGuard guard(&md);
    QCOMPARE(guard.data("text/plain"), QByteArray("any data at all"));
    qunsetenv("COPYQ_CLIPBOARD_MIME_SIZE_LIMIT");
    bumpConfigGeneration();
}

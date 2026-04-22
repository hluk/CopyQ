// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests_clipboarddataguard.h"

#include "common/clipboarddataguard.h"

#include <QMimeData>
#include <QSet>
#include <QTest>
#include <QUrl>

#include <new>

class ThrowingMimeData : public QMimeData {
    Q_OBJECT
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

// Required for ThrowingMimeData's Q_OBJECT macro since it's defined in a .cpp file.
#include "tests_clipboarddataguard.moc"

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

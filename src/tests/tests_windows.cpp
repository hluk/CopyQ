// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"

#include "common/mimetypes.h"

#ifdef Q_OS_WIN

#include <qt_windows.h>
#include <shellapi.h>

#include <vector>

void Tests::createMimeDataHasCfHdrop()
{
    // Realistic text/uri-list content (file:// prefix, forward slashes).
    const QByteArray uriData =
        "file:///C:/Users/test/file1.txt\n"
        "file:///C:/Users/test/file2.txt";

    // Add an item carrying the URI list, then copy it to the system clipboard.
    RUN("write" << "0" << mimeUriList << uriData, "");
    RUN("select" << "0", "");
    WAIT_FOR_CLIPBOARD2(uriData, mimeUriList);

    // Enumerate all native clipboard formats for diagnostic logging.
    QVERIFY2(OpenClipboard(nullptr), "Failed to open clipboard");

    {
        UINT fmt = 0;
        while ((fmt = EnumClipboardFormats(fmt)) != 0) {
            wchar_t name[256] = {};
            GetClipboardFormatNameW(fmt, name, 255);
            qDebug() << "Clipboard format:" << fmt
                     << QString::fromWCharArray(name);
        }
    }

    // Retrieve CF_HDROP.

    HANDLE hDrop = GetClipboardData(CF_HDROP);
    QVERIFY2(hDrop != nullptr, "CF_HDROP not available on clipboard");

    auto *dropFiles = static_cast<HDROP>(hDrop);

    // DragQueryFileW with 0xFFFFFFFF returns the count of files.
    const UINT fileCount = DragQueryFileW(dropFiles, 0xFFFFFFFF, nullptr, 0);
    QCOMPARE(fileCount, UINT(2));

    // Extract and verify each file path.
    auto queryFile = [&](UINT index) -> QString {
        const UINT len = DragQueryFileW(dropFiles, index, nullptr, 0);
        std::vector<wchar_t> buf(len + 1);
        DragQueryFileW(dropFiles, index, buf.data(), len + 1);
        return QString::fromWCharArray(buf.data(), len);
    };

    // DragQueryFileW returns native Windows paths (backslashes).
    QCOMPARE(queryFile(0), QStringLiteral("C:\\Users\\test\\file1.txt"));
    QCOMPARE(queryFile(1), QStringLiteral("C:\\Users\\test\\file2.txt"));

    CloseClipboard();
}

#else // !Q_OS_WIN

void Tests::createMimeDataHasCfHdrop()
{
    QSKIP("CF_HDROP is a Windows-only clipboard format");
}

#endif // Q_OS_WIN

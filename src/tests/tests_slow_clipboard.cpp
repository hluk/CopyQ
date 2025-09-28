// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_utils.h"
#include "tests.h"
#include "tests_common.h"

#include "common/log.h"
#include "common/sleeptimer.h"
#include "platform/platformclipboard.h"
#include "platform/platformnativeinterface.h"

#include <QMimeData>

namespace {

class SlowMimeData final : public QMimeData {
public:
    explicit SlowMimeData(const QByteArray &data, int delayMs)
        : m_data(data)
        , m_delayMs(delayMs)
    {}

    bool hasFormat(const QString &mimeType) const override
    {
        return formats().contains(mimeType);
    }

    QStringList formats() const override
    {
        return {"a/a", "b/b", "c/c"};
    }

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QVariant retrieveData(const QString &mimeType, QMetaType) const override
#else
    QVariant retrieveData(const QString &mimeType, QVariant::Type) const override
#endif
    {
        if (formats().contains(mimeType)) {
            waitFor(m_delayMs);
            return m_data;
        }
        return {};
    }
private:
    QByteArray m_data;
    int m_delayMs;
};

} // namespace

void Tests::slowClipboard()
{
    const auto script = R"(
        setCommands([
            {
                isScript: true,
                cmd: 'global.clipboardFormatsToSave = function() { return ["a/a", "b/b", "c/c"] }'
            },
        ])
        )";
    RUN(script, "");
    WAIT_ON_OUTPUT("clipboardFormatsToSave", "a/a\nb/b\nc/c\n");

    TEST( m_test->setClipboard("A", "a/a") );
    WAIT_ON_OUTPUT("clipboard" << "a/a", "A");

    auto clipboard = platformNativeInterface()->clipboard();
    for (int i = 0; i < 3; ++i) {
        QMimeData *data = new SlowMimeData(QByteArray::number(i), i == 2 ? 100 : 1000);
        clipboard->setRawData(ClipboardMode::Clipboard, data);
        waitFor(50);
    }
    WAIT_ON_OUTPUT("read('a/a', 0, 1, 2, 3)", "2\nA\n\n");
    RUN("read('b/b', 0)", "2");
    RUN("read('c/c', 0)", "2");
    RUN("read('?', 0)", "a/a\nb/b\nc/c\n");

    QMimeData *data = new SlowMimeData("X", 1500);
    clipboard->setRawData(ClipboardMode::Clipboard, data);
    waitFor(2000);
    const auto expectedLog = R"(^.*<cmd/monitorClipboard-\d+>: Aborting clipboard cloning: Data access took too long$)";
    QTRY_COMPARE( count(splitLines(readLogFile(maxReadLogSize)), expectedLog), 1 );

    TEST( m_test->setClipboard("B", "a/a") );
    WAIT_ON_OUTPUT("clipboard" << "a/a", "B");
    RUN("read('a/a', 0, 1, 2, 3)", "B\n2\nA\n");
}

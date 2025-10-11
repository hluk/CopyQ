// SPDX-License-Identifier: GPL-3.0-or-later

#include "messagehandlerforqt.h"

#include "common/log.h"

#include <QLoggingCategory>
#include <QString>
#include <QtGlobal>

#include <exception>

namespace {

class ExceptionQtFatal final : public std::exception {
public:
    explicit ExceptionQtFatal(const QByteArray &message)
        : m_message(message)
    {
    }

    const char* what() const noexcept override
    {
        return m_message.constData();
    }

private:
    QByteArray m_message;
};

void messageHandlerForQt(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString message = msg;
    if (context.file && context.file[0]) {
        message.append(
            QStringLiteral(" (%1:%2, %3)")
                .arg(context.file, QString::number(context.line), context.function));
    }

    const QString format = QStringLiteral("[%1] %2");
    const QLatin1String category(context.category);
    switch (type) {
    case QtDebugMsg:
        log( format.arg(category, message), LogDebug);
        break;
    case QtInfoMsg:
        log( format.arg(category, message), LogDebug);
        break;
    case QtWarningMsg:
        log( format.arg(category, message), LogWarning);
        break;
    case QtCriticalMsg:
        log( format.arg(category, message), LogError);
        break;
    case QtFatalMsg:
        log( format.arg(category, message), LogError);
        throw ExceptionQtFatal( message.toUtf8() );
    }
}

} // namespace

void installMessageHandlerForQt()
{
    switch(getLogLevel()) {
    case LogDebug:
    case LogTrace:
        QLoggingCategory::setFilterRules("copyq.*=true");
        break;

    case LogNote:
    case LogAlways:
        QLoggingCategory::setFilterRules(
            "copyq.*.info=true\ncopyq.*.warning=true\ncopyq.*.critical=true");
        break;

    case LogWarning:
        QLoggingCategory::setFilterRules(
            "copyq.*.warning=true\ncopyq.*.critical=true");
        break;

    case LogError:
        QLoggingCategory::setFilterRules("copyq.*.critical=true");
        break;
    }
    qInstallMessageHandler(messageHandlerForQt);
}

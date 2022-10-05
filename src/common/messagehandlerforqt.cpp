// SPDX-License-Identifier: GPL-3.0-or-later

#include "messagehandlerforqt.h"

#include "common/log.h"

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

void messageHandlerForQt5(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString message = msg;
    if (context.file && context.file[0]) {
        message.append(
            QStringLiteral(" (%1:%2, %3)")
                .arg(context.file, QString::number(context.line), context.function));
    }

    const QString format = QStringLiteral("[%1] %3: %2");
    const QLatin1String category(context.category);
    switch (type) {
    case QtDebugMsg:
        log( format.arg(category, message, QStringLiteral("QtDebug")), LogDebug);
        break;
    case QtInfoMsg:
        log( format.arg(category, message, QStringLiteral("QtInfo")), LogDebug);
        break;
    case QtWarningMsg:
        log( format.arg(category, message, QStringLiteral("QtWarning")), LogWarning);
        break;
    case QtCriticalMsg:
        log( format.arg(category, message, QStringLiteral("QtCritical")), LogError);
        break;
    case QtFatalMsg:
        log( format.arg(category, message, QStringLiteral("QtFatal")), LogError);
        throw ExceptionQtFatal( message.toUtf8() );
    }
}

} // namespace

void installMessageHandlerForQt()
{
    qInstallMessageHandler(messageHandlerForQt5);
}

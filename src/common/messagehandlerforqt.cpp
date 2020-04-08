/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

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

void messageHandler(QtMsgType type, const QString &message)
{
    switch (type) {
    case QtDebugMsg:
        log("QtDebug: " + message, LogDebug);
        break;
#if QT_VERSION >= 0x050500
    case QtInfoMsg:
        log("QtInfo: " + message, LogDebug);
        break;
#endif
    case QtWarningMsg:
        log("QtWarning: " + message, LogDebug);
        break;
    case QtCriticalMsg:
        log("QtCritical: " + message, LogError);
        break;
    case QtFatalMsg:
        log("QtFatal: " + message, LogError);
        throw ExceptionQtFatal( message.toUtf8() );
    }
}

void messageHandlerForQt5(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString message = QString("%1 (%2:%3, %4)")
            .arg(msg, context.file, QString::number(context.line), context.function);
    messageHandler(type, message);
}

} // namespace

void installMessageHandlerForQt()
{
    qInstallMessageHandler(messageHandlerForQt5);
}

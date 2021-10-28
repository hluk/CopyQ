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

void messageHandlerForQt5(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString message = msg;
    if ( context.file != QLatin1String() ) {
        message.append(
            QString::fromLatin1(" (%1:%2, %3)")
                .arg(context.file, QString::number(context.line), context.function));
    }

    const QString format = QString::fromLatin1("[%1] %3: %2");
    const QLatin1String category(context.category);
    switch (type) {
    case QtDebugMsg:
        log( format.arg(category, message, QLatin1String("QtDebug")), LogDebug);
        break;
    case QtInfoMsg:
        log( format.arg(category, message, QLatin1String("QtInfo")), LogDebug);
        break;
    case QtWarningMsg:
        log( format.arg(category, message, QLatin1String("QtWarning")), LogWarning);
        break;
    case QtCriticalMsg:
        log( format.arg(category, message, QLatin1String("QtCritical")), LogError);
        break;
    case QtFatalMsg:
        log( format.arg(category, message, QLatin1String("QtFatal")), LogError);
        throw ExceptionQtFatal( message.toUtf8() );
    }
}

} // namespace

void installMessageHandlerForQt()
{
    qInstallMessageHandler(messageHandlerForQt5);
}

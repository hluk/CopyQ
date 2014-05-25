/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "item/encrypt.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegExp>
#include <QSettings>
#include <QString>
#include <QStringList>

const char mimeEncryptedData[] = "application/x-copyq-encrypted";

QString getConfigurationFilePath(const QString &suffix)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    QString path = settings.fileName();
    return path.replace( QRegExp("\\.ini$"), suffix );
}

QStringList getDefaultEncryptCommandArguments()
{
    KeyPairPaths keys;
    return QStringList() << "--trust-model" << "always" << "--recipient" << "copyq"
                         << "--charset" << "utf-8" << "--display-charset" << "utf-8" << "--no-tty"
                         << "--no-default-keyring" << "--secret-keyring" << keys.sec << "--keyring" << keys.pub;
}

QString getEncryptCommand()
{
    QStringList args = getDefaultEncryptCommandArguments();

    // Escape characters
    for ( int i = 0; i < args.size(); ++i ) {
        args[i].replace("\\", "\\\\")
               .replace(" ", "\\ ")
               .replace("\"", "\\\"");
    }

    return "gpg " + args.join(" ");
}

KeyPairPaths::KeyPairPaths()
{
    const QString path = getConfigurationFilePath(QString());
    sec = QDir::toNativeSeparators(path + ".sec");
    pub = QDir::toNativeSeparators(path + ".pub");
}

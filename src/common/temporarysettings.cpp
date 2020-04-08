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

#include "temporarysettings.h"

#include "common/log.h"
#include "common/temporaryfile.h"

#include <QTemporaryFile>

namespace {

QString temporaryFileName(const QByteArray &content)
{
    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile, ".ini") )
    {
        log("Failed to create temporary settings file", LogError);
        return QString();
    }

    tmpfile.write(content);
    tmpfile.setAutoRemove(false);
    return tmpfile.fileName();
}

} // namespace

TemporarySettings::TemporarySettings(const QByteArray &content)
    : m_settings(new QSettings(temporaryFileName(content), QSettings::IniFormat))
{
}

TemporarySettings::~TemporarySettings()
{
    const QString fileName = m_settings->fileName();
    delete m_settings;
    m_settings = nullptr;

    if (!fileName.isEmpty())
        QFile::remove(fileName);
}

QSettings *TemporarySettings::settings()
{
    return m_settings;
}

QByteArray TemporarySettings::content()
{
    m_settings->sync();

    QFile file(m_settings->fileName());
    if (!file.open(QIODevice::ReadOnly)) {
        log("Failed to open temporary settings file", LogError);
        return QByteArray();
    }

    return file.readAll();
}

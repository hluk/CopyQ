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

#include "temporaryfile.h"

#include "common/log.h"

#include <QDir>
#include <QTemporaryFile>

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix)
{
    const QString tmpFileName = "CopyQ.XXXXXX" + suffix;
    const QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);
    file->setFileTemplate(tmpPath);

    if ( !file->open() ) {
        log( QString("Failed to open temporary file \"%1\" (template \"%2\")")
             .arg(file->fileName(), tmpPath),
             LogError );
        return true;
    }

    if ( !file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) ) {
        log( QString("Failed set permisssions to temporary file \"%1\"")
             .arg(file->fileName()), LogError );
        return false;
    }

    return true;
}

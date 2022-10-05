// SPDX-License-Identifier: GPL-3.0-or-later

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
        return false;
    }

    if ( !file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) ) {
        log( QString("Failed set permissions to temporary file \"%1\"")
             .arg(file->fileName()), LogError );
        return false;
    }

    return true;
}

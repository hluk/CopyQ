// SPDX-License-Identifier: GPL-3.0-or-later

#include "temporaryfile.h"

#include <QDir>
#include <QDebug>
#include <QTemporaryFile>

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix)
{
    const QString tmpFileName = "CopyQ.XXXXXX" + suffix;
    const QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);
    file->setFileTemplate(tmpPath);

    if ( !file->open() ) {
        qCritical() << "Failed to open temporary file"
            << file->fileName() << "template" << tmpPath << ":" << file->errorString();
        return false;
    }

    if ( !file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) ) {
        qCritical() << "Failed set permissions to temporary file"
            << file->fileName() << ":" << file->errorString();
        return false;
    }

    return true;
}

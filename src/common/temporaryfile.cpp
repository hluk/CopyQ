// SPDX-License-Identifier: GPL-3.0-or-later

#include "temporaryfile.h"

#include <QDir>
#include <QLoggingCategory>
#include <QTemporaryFile>

namespace {

Q_DECLARE_LOGGING_CATEGORY(tempFileCategory)
Q_LOGGING_CATEGORY(tempFileCategory, "copyq.temporaryfile")

} // namespace

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix)
{
    const QString tmpFileName = "CopyQ.XXXXXX" + suffix;
    const QString tmpPath = QDir( QDir::tempPath() ).absoluteFilePath(tmpFileName);
    file->setFileTemplate(tmpPath);

    if ( !file->open() ) {
        qCCritical(tempFileCategory) << "Failed to open temporary file"
            << file->fileName() << "template" << tmpPath << ":" << file->errorString();
        return false;
    }

    if ( !file->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner) ) {
        qCCritical(tempFileCategory) << "Failed set permissions to temporary file"
            << file->fileName() << ":" << file->errorString();
        return false;
    }

    return true;
}

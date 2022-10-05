// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TEMPORARYFILE_H
#define TEMPORARYFILE_H

class QString;
class QTemporaryFile;

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix);

#endif // TEMPORARYFILE_H

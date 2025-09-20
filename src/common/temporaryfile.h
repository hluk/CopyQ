// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once


class QString;
class QTemporaryFile;

bool openTemporaryFile(QTemporaryFile *file, const QString &suffix);

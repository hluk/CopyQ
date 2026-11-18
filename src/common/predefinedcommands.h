// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QString>
#include <QtContainerFwd>

struct Command;

QVector<Command> predefinedCommands();

/**
 * Script that saves input() image data to a temporary file and stores its path
 * in `path` variable. Also removes such files older than 24 hours.
 */
QString pasteImageAsFilePathScript();

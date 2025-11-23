// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

bool ensureSettingsDirectoryExists();

const QString &getConfigurationFilePath();

QString getConfigurationFilePath(const char *suffix);

const QString &settingsDirectoryPath();

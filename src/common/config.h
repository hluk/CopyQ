// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

bool ensureSettingsDirectoryExists();

const QString &getConfigurationFilePath();

QString getConfigurationFilePath(const char *suffix);

const QString &settingsDirectoryPath();

/**
 * Returns the path to the CopyQ executable. When running inside an AppImage,
 * returns the AppImage path ($APPIMAGE) so that child processes go through
 * the AppRun wrapper and inherit the correct library environment.
 */
QString applicationExecutablePath();

/**
 * Adjusts a compile-time absolute path for AppImage: when $APPDIR is set,
 * prepends it so that bundled resources (themes, translations, plugins)
 * are found at their mount-point location.
 */
QString appImageAdjustedPath(const QString &compiledPath);

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

bool ensureSettingsDirectoryExists();

const QString &getConfigurationFilePath();

QString getConfigurationFilePath(const char *suffix);

const QString &settingsDirectoryPath();

/**
 * Returns the path to the CopyQ executable. When built with AppImage support
 * and running inside an AppImage, returns the AppImage path ($APPIMAGE) so
 * that child processes go through the AppRun wrapper.
 */
QString applicationExecutablePath();

/**
 * Adjusts a compile-time install path for the runtime environment.
 * With AppImage support, prepends $APPDIR so bundled resources are found.
 * Without AppImage support, returns the path unchanged.
 */
QString adjustedInstallPath(const QString &compiledPath);

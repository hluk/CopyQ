// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QString;

bool ensureSettingsDirectoryExists();

const QString &getConfigurationFilePath();

QString getConfigurationFilePath(const char *suffix);

const QString &settingsDirectoryPath();

/**
 * Returns the path to launch CopyQ from outside the current process.
 *
 * Inside an AppImage, the running binary lives on a temporary FUSE mount
 * (e.g. /tmp/.mount_CopyQXXXXXX/usr/bin/copyq) that disappears when the
 * AppImage exits. Callers that persist the path -- the $COPYQ env var
 * exported to user scripts, and the Exec= line in the autostart desktop
 * file -- need the stable AppImage file path ($APPIMAGE) instead.
 *
 * When $APPIMAGE is unset (non-AppImage build or env var not propagated),
 * falls back to QCoreApplication::applicationFilePath().
 */
QString applicationLaunchPath();

/**
 * Adjusts a compile-time install path for the runtime environment.
 * With AppImage support, prepends $APPDIR so bundled resources are found.
 * Without AppImage support, returns the path unchanged.
 */
QString adjustedInstallPath(const QString &compiledPath);

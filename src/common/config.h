// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIG_H
#define CONFIG_H

class QByteArray;
class QPoint;
class QString;
class QVariant;
class QWidget;

bool ensureSettingsDirectoryExists();

const QString &getConfigurationFilePath();

QString getConfigurationFilePath(const char *suffix);

const QString &settingsDirectoryPath();

QVariant geometryOptionValue(const QString &optionName);
void setGeometryOptionValue(const QString &optionName, const QVariant &value);

void restoreWindowGeometry(QWidget *w, bool openOnCurrentScreen);

void saveWindowGeometry(QWidget *w, bool openOnCurrentScreen);

QByteArray mainWindowState(const QString &mainWindowObjectName);

void saveMainWindowState(const QString &mainWindowObjectName, const QByteArray &state);

void moveToCurrentWorkspace(QWidget *w);

void moveWindowOnScreen(QWidget *w, QPoint pos);

void setGeometryGuardBlockedUntilHidden(QWidget *w, bool blocked);
bool isGeometryGuardBlockedUntilHidden(const QWidget *w);

#endif // CONFIG_H

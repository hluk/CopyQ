// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSettings>

class QString;

/**
 * Simple wrapper class for QSettings.
 */
class Settings final : public QSettings
{
public:
    Settings();

    explicit Settings(const QString &path);

    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;
};

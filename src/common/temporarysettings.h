// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


class QByteArray;
class QSettings;

/**
 * Temporary ini settings which is removed after destroyed.
 *
 * Use this to get ini as data instead of saving to a file.
 */
class TemporarySettings final
{
public:
    /// Creates temporary settings file.
    explicit TemporarySettings(const QByteArray &content);

    /// Destroys underlying settings and removes settings file.
    ~TemporarySettings();

    /// Returns underlying settings.
    QSettings *settings();

    /// Return content of settings file.
    QByteArray content();

    TemporarySettings(const TemporarySettings &) = delete;
    TemporarySettings &operator=(const TemporarySettings &) = delete;

private:
    QSettings *m_settings;
};

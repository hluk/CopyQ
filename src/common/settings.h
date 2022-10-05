// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QString>

/**
 * Wrapper for safer QSettings() handling.
 *
 * For more safety, this should be instantiated only from main application thread.
 *
 * Stores and reads values from application settings copy. Updates the application
 * settings when destroyed.
 *
 * Special file is used to designate whether application settings or its copy is valid.
 *
 * While special file doesn't exist, the application settings is valid.
 *
 * While special file exists, copy is valid and application settings is not.
 *
 * When created:
 *   - if last save was unsuccessful, restores copy of application settings,
 *   - if last save was successful, copies application settings and uses this copy.
 *
 * When destroyed:
 *   - synchronizes the underlying copy of application settings,
 *   - creates special file,
 *   - copies settings from copy to real application settings and flushes it,
 *   - deletes special file.
 */
class Settings final
{
public:
    static bool canModifySettings;

    static bool isEmpty(const QSettings &settings);

    Settings();

    explicit Settings(const QString &path);

    ~Settings();

    bool isEmpty() const { return isEmpty(m_settings); }

    bool contains(const QString &name) const { return m_settings.contains(name); }

    QVariant value(const QString &name) const { return m_settings.value(name); }

    void setValue(const QString &name, const QVariant &value) {
        m_changed = true;
        m_settings.setValue(name, value);
    }

    void remove(const QString &name) {
        m_changed = true;
        m_settings.remove(name);
    }

    void clear() {
        m_changed = true;
        m_settings.clear();
    }

    void beginGroup(const QString &prefix) {
        m_settings.beginGroup(prefix);
    }

    void endGroup() {
        m_settings.endGroup();
    }

    QStringList allKeys() {
        return m_settings.allKeys();
    }

    int beginReadArray(const QString &prefix) {
        return m_settings.beginReadArray(prefix);
    }

    void beginWriteArray(const QString &prefix, int size = -1) {
        m_changed = true;
        m_settings.beginWriteArray(prefix, size);
    }

    void endArray() {
        m_settings.endArray();
    }

    void setArrayIndex(int i) {
        m_settings.setArrayIndex(i);
    }

    QString fileName() const {
        return m_settings.fileName();
    }

    QSettings *settingsData() {
        m_changed = true;
        return &m_settings;
    }

    QSettings &constSettingsData() { return m_settings; }

    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;

    void restore();

private:
    void restore(const QSettings &settings);

    void save();

    QSettings m_settings;

    /// True only if QSetting data changed and need to be synced.
    bool m_changed;

    QString m_path;
};


#endif // SETTINGS_H

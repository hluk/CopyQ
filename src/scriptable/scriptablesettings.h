// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLESETTINGS_H
#define SCRIPTABLESETTINGS_H

#include <QJSValue>
#include <QObject>
#include <QSettings>

class ScriptableSettings final : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ScriptableSettings();
    Q_INVOKABLE explicit ScriptableSettings(const QString &fileName);

public slots:
    QStringList allKeys() { return m_settings.allKeys(); }
    void beginGroup(const QString &prefix) { m_settings.beginGroup(prefix); }
    QJSValue beginReadArray(const QString &prefix) { return m_settings.beginReadArray(prefix); }
    void beginWriteArray(const QString &prefix, int size = -1) { m_settings.beginWriteArray(prefix, size); }
    QStringList childGroups() { return m_settings.childGroups(); }
    QStringList childKeys() { return m_settings.childKeys(); }
    void clear() { m_settings.clear(); }
    QJSValue contains(const QString &key) { return m_settings.contains(key); }
    void endArray() { m_settings.endArray(); }
    void endGroup() { m_settings.endGroup(); }
    QJSValue fileName() { return m_settings.fileName(); }
    QJSValue group() { return m_settings.group(); }
    QJSValue isWritable() { return m_settings.isWritable(); }
    void remove(const QString &key) { m_settings.remove(key); }
    void setArrayIndex(int i) { m_settings.setArrayIndex(i); }
    void setValue(const QString &key, const QJSValue &value) { m_settings.setValue(key, value.toVariant()); }
    void sync() { m_settings.sync(); }
    QVariant value(const QString &key, const QJSValue &defaultValue = QJSValue()) { return m_settings.value(key, defaultValue.toVariant()); }

private:
    QSettings m_settings;
};

#endif // SCRIPTABLESETTINGS_H

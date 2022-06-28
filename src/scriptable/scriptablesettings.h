/*
    Copyright (c) 2022, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

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

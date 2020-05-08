/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#pragma once

#include "common/clipboardmode.h"
#include "common/command.h"
#include "gui/notificationbutton.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QVariant>
#include <QVector>

#include <unordered_map>

class ClipboardBrowser;
class KeyClicker;
class MainWindow;
class QEventLoop;
class QPersistentModelIndex;
class QPixmap;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

using NamedValueList = QVector<NamedValue>;

struct ScriptablePath {
    QString path;
};

Q_DECLARE_METATYPE(NamedValueList)
Q_DECLARE_METATYPE(ScriptablePath)
Q_DECLARE_METATYPE(NotificationButtons)
Q_DECLARE_METATYPE(QVector<QVariantMap>)
Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(Command)
Q_DECLARE_METATYPE(ClipboardMode)

QDataStream &operator<<(QDataStream &out, const NotificationButton &button);
QDataStream &operator>>(QDataStream &in, NotificationButton &button);
QDataStream &operator<<(QDataStream &out, const NamedValueList &list);
QDataStream &operator>>(QDataStream &in, NamedValueList &list);
QDataStream &operator<<(QDataStream &out, const Command &command);
QDataStream &operator>>(QDataStream &in, Command &command);
QDataStream &operator<<(QDataStream &out, ClipboardMode mode);
QDataStream &operator>>(QDataStream &in, ClipboardMode &mode);
QDataStream &operator<<(QDataStream &out, const ScriptablePath &path);
QDataStream &operator>>(QDataStream &in, ScriptablePath &path);
QDataStream &operator<<(QDataStream &out, Qt::KeyboardModifiers value);
QDataStream &operator>>(QDataStream &in, Qt::KeyboardModifiers &value);

QString pluginsPath();
QString themesPath();
QString translationsPath();

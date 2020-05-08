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

#include "scriptableproxy-client.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/command.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/sleeptimer.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QDir>
#include <QMetaMethod>
#include <QTimer>

#include <type_traits>

const quint32 serializedFunctionCallMagicNumber = 0x58746908;
const quint32 serializedFunctionCallVersion = 2;

Q_DECLARE_METATYPE(QFile*)

QDataStream &operator<<(QDataStream &out, const NotificationButton &button)
{
    out << button.name
        << button.script
        << button.data;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NotificationButton &button)
{
    in >> button.name
       >> button.script
       >> button.data;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const NamedValueList &list)
{
    out << list.size();
    for (const auto &item : list)
        out << item.name << item.value;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NamedValueList &list)
{
    int size;
    in >> size;
    for (int i = 0; i < size; ++i) {
        NamedValue item;
        in >> item.name >> item.value;
        list.append(item);
    }
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const Command &command)
{
    out << command.name
        << command.re
        << command.wndre
        << command.matchCmd
        << command.cmd
        << command.sep
        << command.input
        << command.output
        << command.wait
        << command.automatic
        << command.display
        << command.inMenu
        << command.isGlobalShortcut
        << command.isScript
        << command.transform
        << command.remove
        << command.hideWindow
        << command.enable
        << command.icon
        << command.shortcuts
        << command.globalShortcuts
        << command.tab
        << command.outputTab;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, Command &command)
{
    in >> command.name
       >> command.re
       >> command.wndre
       >> command.matchCmd
       >> command.cmd
       >> command.sep
       >> command.input
       >> command.output
       >> command.wait
       >> command.automatic
       >> command.display
       >> command.inMenu
       >> command.isGlobalShortcut
       >> command.isScript
       >> command.transform
       >> command.remove
       >> command.hideWindow
       >> command.enable
       >> command.icon
       >> command.shortcuts
       >> command.globalShortcuts
       >> command.tab
       >> command.outputTab;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, ClipboardMode mode)
{
    const int modeId = static_cast<int>(mode);
    out << modeId;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, ClipboardMode &mode)
{
    int modeId;
    in >> modeId;
    Q_ASSERT(in.status() == QDataStream::Ok);
    mode = static_cast<ClipboardMode>(modeId);
    return in;
}

QDataStream &operator<<(QDataStream &out, const ScriptablePath &path)
{
    return out << path.path;
}

QDataStream &operator>>(QDataStream &in, ScriptablePath &path)
{
    return in >> path.path;
}

QDataStream &operator<<(QDataStream &out, Qt::KeyboardModifiers value)
{
    return out << static_cast<int>(value);
}

QDataStream &operator>>(QDataStream &in, Qt::KeyboardModifiers &value)
{
    int valueInt;
    in >> valueInt;
    Q_ASSERT(in.status() == QDataStream::Ok);
    value = static_cast<Qt::KeyboardModifiers>(valueInt);
    return in;
}

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

struct InputDialog {
    QDialog *dialog = nullptr;
    QString defaultChoice; /// Default text for list widgets.
};

class FunctionCallSerializer final {
public:
    explicit FunctionCallSerializer(const char *functionName)
    {
        m_slotName = functionName;
    }

    template<typename ...Ts>
    FunctionCallSerializer &withSlotArguments(Ts... arguments)
    {
        QByteArray args;
        for (const auto argType : std::initializer_list<const char *>{ argumentType(arguments)... }) {
            args.append(argType);
            args.append(',');
        }
        args.chop(1);
        setSlotArgumentTypes(args);
        return *this;
    }

    QByteArray serialize(int functionCallId, const QVector<QVariant> args) const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_0);
        stream << serializedFunctionCallMagicNumber << serializedFunctionCallVersion
               << functionCallId << m_slotName << args;
        return bytes;
    }

    template<typename ...Ts>
    static QVector<QVariant> argumentList(Ts... arguments)
    {
        return { QVariant::fromValue(arguments)... };
    }

    template<typename T>
    static const char *argumentType(const T &)
    {
        if ( std::is_same<QVariant, T>::value )
            return "QVariant";

        return QMetaType::typeName(qMetaTypeId<T>());
    }

private:
    void setSlotArgumentTypes(const QByteArray &args)
    {
        m_slotName += "(" + args + ")";
        const int slotIndex = ScriptableProxyClient::staticMetaObject.indexOfSlot(m_slotName);
        if (slotIndex == -1) {
            log("Failed to find scriptable proxy slot: " + m_slotName, LogError);
            Q_ASSERT(false);
        }
    }

    QByteArray m_slotName;
};

} // namespace

QString pluginsPath()
{
    QDir dir;
    if (platformNativeInterface()->findPluginDir(&dir))
        return dir.absolutePath();

    return QString();
}

QString themesPath()
{
    return platformNativeInterface()->themePrefix();
}

QString translationsPath()
{
    return platformNativeInterface()->translationPrefix();
}

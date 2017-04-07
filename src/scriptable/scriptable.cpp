/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "scriptable.h"

#include "common/action.h"
#include "common/arguments.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "item/serialize.h"
#include "gui/icons.h"
#include "scriptable/commandhelp.h"
#include "scriptable/scriptablebytearray.h"
#include "scriptable/scriptabledir.h"
#include "scriptable/scriptablefile.h"
#include "scriptable/scriptableproxy.h"
#include "scriptable/scriptabletemporaryfile.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QDesktopServices>
#include <QDir>
#include <QJSEngine>
#include <QJSValueIterator>
#include <QMetaType>
#include <QNetworkReply>
#include <QSettings>
#include <QTextCodec>
#include <QThread>

namespace {

const char *const programName = "CopyQ Clipboard Manager";

const int setClipboardMaxRetries = 3;

QString helpHead()
{
    return Scriptable::tr("Usage: copyq [%1]").arg(Scriptable::tr("COMMAND")) + "\n\n"
        + Scriptable::tr("Starts server if no command is specified.") + "\n"
        + Scriptable::tr("  COMMANDs:");
}

QString helpTail()
{
    return Scriptable::tr("NOTES:") + "\n"
        + Scriptable::tr("  - Use dash argument (-) to read data from standard input.") + "\n"
        + Scriptable::tr("  - Use double-dash argument (--) to read all following arguments without\n"
                      "    expanding escape sequences (i.e. \\n, \\t and others).") + "\n"
        + Scriptable::tr("  - Use ? for MIME to print available MIME types (default is \"text/plain\").");
}

QString argumentError()
{
    return Scriptable::tr("Invalid number of arguments!");
}

bool clipboardContains(
        QClipboard::Mode mode, ScriptableProxy *proxy,
        const QString &format, const QByteArray &content)
{
    return content == proxy->getClipboardData(format, mode);
}

QJSValue newByteArray(const QByteArray &bytes, const Scriptable *scriptable)
{
    return scriptable->engine()->newQObject( new ScriptableByteArray(bytes) );
}

const QByteArray *getByteArray(const QJSValue &value, const Scriptable *scriptable)
{
    const auto scriptableBytes = scriptable->engine()->fromScriptValue<ScriptableByteArray*>(value);
    return scriptableBytes ? scriptableBytes->data() : nullptr;
}

QFile *getFile(const QJSValue &value, const Scriptable *scriptable)
{
    const auto scriptableFile = scriptable->engine()->fromScriptValue<ScriptableFile*>(value);
    return scriptableFile ? scriptableFile->self() : nullptr;
}

QString toString(const QJSValue &value, const Scriptable *scriptable)
{
    const auto bytes = getByteArray(value, scriptable);
    return bytes ? getTextData(*bytes) : value.toString();
}

QByteArray toByteArray(const QJSValue &value, const Scriptable *scriptable)
{
    const auto bytes = getByteArray(value, scriptable);
    return bytes ? *bytes : value.toString().toUtf8();
}

QVariant toVariant(const QJSValue &value)
{
    const auto variant = value.toVariant();
    Q_ASSERT(value.isUndefined() || value.isNull() || variant.isValid());
    return variant;
}

template <typename T>
struct ScriptValueFactory {
    static QJSValue toScriptValue(const T &value, const Scriptable *)
    {
        return QJSValue(value);
    }

    static T fromScriptValue(const QJSValue &value, const Scriptable *)
    {
        const auto variant = toVariant(value);
        Q_ASSERT( variant.canConvert<T>() );
        return variant.value<T>();
    }
};

template <typename T>
QJSValue toScriptValue(const T &value, const Scriptable *scriptable)
{
    return ScriptValueFactory<T>::toScriptValue(value, scriptable);
}

template <typename T>
T fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
{
    return ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename T>
void fromScriptValueDefined(const QJSValue &value, const Scriptable *scriptable, T *outputValue)
{
    if (!value.isUndefined())
        *outputValue = ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename T>
struct ScriptValueFactory< QList<T> > {
    static QJSValue toScriptValue(const QList<T> &list, const Scriptable *scriptable)
    {
        QJSValue array = scriptable->engine()->newArray();
        for ( int i = 0; i < list.size(); ++i ) {
            const auto value = ScriptValueFactory<T>::toScriptValue(list[i], scriptable);
            array.setProperty( static_cast<quint32>(i), value );
        }
        return array;
    }

    static QList<T> fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        if ( !value.isArray() )
            return QList<T>();

        const quint32 length = value.property("length").toUInt();
        QList<T> list;
        for ( quint32 i = 0; i < length; ++i ) {
            const auto item = value.property(i);
            list.append( ScriptValueFactory<T>::fromScriptValue(item, scriptable) );
        }
        return list;
    }
};

template <>
struct ScriptValueFactory<QVariantMap> {
    static QJSValue toScriptValue(const QVariantMap &dataMap, const Scriptable *scriptable)
    {
        QJSValue value = scriptable->engine()->newObject();

        for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it)
            value.setProperty( it.key(), ::toScriptValue(it.value(), scriptable) );

        return value;
    }

    static QVariantMap fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        QVariantMap result;
        QJSValueIterator it(value);
        while ( it.hasNext() ) {
            it.next();

            const auto name = it.name();
            if (name == "prototype" || name == "constructor")
                continue;

            const auto itemValue = ::fromScriptValue<QVariant>( it.value(), scriptable );
            result.insert(name, itemValue);
        }
        return result;
    }
};

template <>
struct ScriptValueFactory<QByteArray> {
    static QJSValue toScriptValue(const QByteArray &bytes, const Scriptable *scriptable)
    {
        return newByteArray(bytes, scriptable);
    }
};

template <>
struct ScriptValueFactory<QStringList> {
    static QJSValue toScriptValue(const QStringList &list, const Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::toScriptValue(list, scriptable);
    }

    static QStringList fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::fromScriptValue(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QString> {
    static QJSValue toScriptValue(const QString &text, const Scriptable *)
    {
        return QJSValue(text);
    }

    static QString fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        return toString(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QRegExp> {
    static QJSValue toScriptValue(const QRegExp &re, const Scriptable *)
    {
        return QJSValue(re.pattern());
    }

    static QRegExp fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        return value.isRegExp()
                ? value.toVariant().toRegExp()
                : QRegExp( toString(value, scriptable) );
    }
};

template <>
struct ScriptValueFactory<Command> {
    static QJSValue toScriptValue(const Command &command, const Scriptable *scriptable)
    {
        QJSValue value = scriptable->engine()->newObject();

        value.setProperty("name", command.name);
        value.setProperty("re", ::toScriptValue(command.re, scriptable));
        value.setProperty("wndre", ::toScriptValue(command.wndre, scriptable));
        value.setProperty("matchCmd", command.matchCmd);
        value.setProperty("cmd", command.cmd);
        value.setProperty("sep", command.sep);
        value.setProperty("input", command.input);
        value.setProperty("output", command.output);
        value.setProperty("wait", command.wait);
        value.setProperty("automatic", command.automatic);
        value.setProperty("inMenu", command.inMenu);
        value.setProperty("transform", command.transform);
        value.setProperty("remove", command.remove);
        value.setProperty("hideWindow", command.hideWindow);
        value.setProperty("enable", command.enable);
        value.setProperty("icon", command.icon);
        value.setProperty("shortcuts", ::toScriptValue(command.shortcuts, scriptable));
        value.setProperty("globalShortcuts", ::toScriptValue(command.globalShortcuts, scriptable));
        value.setProperty("tab", command.tab);
        value.setProperty("outputTab", command.outputTab);

        return value;
    }

    static Command fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        Command command;

        ::fromScriptValueDefined( value.property("name"), scriptable, &command.name );
        ::fromScriptValueDefined( value.property("re"), scriptable, &command.re );
        ::fromScriptValueDefined( value.property("wndre"), scriptable, &command.wndre );
        ::fromScriptValueDefined( value.property("matchCmd"), scriptable, &command.matchCmd );
        ::fromScriptValueDefined( value.property("cmd"), scriptable, &command.cmd );
        ::fromScriptValueDefined( value.property("sep"), scriptable, &command.sep );
        ::fromScriptValueDefined( value.property("input"), scriptable, &command.input );
        ::fromScriptValueDefined( value.property("output"), scriptable, &command.output );
        ::fromScriptValueDefined( value.property("wait"), scriptable, &command.wait );
        ::fromScriptValueDefined( value.property("automatic"), scriptable, &command.automatic );
        ::fromScriptValueDefined( value.property("inMenu"), scriptable, &command.inMenu );
        ::fromScriptValueDefined( value.property("transform"), scriptable, &command.transform );
        ::fromScriptValueDefined( value.property("remove"), scriptable, &command.remove );
        ::fromScriptValueDefined( value.property("hideWindow"), scriptable, &command.hideWindow );
        ::fromScriptValueDefined( value.property("enable"), scriptable, &command.enable );
        ::fromScriptValueDefined( value.property("icon"), scriptable, &command.icon );
        ::fromScriptValueDefined( value.property("shortcuts"), scriptable, &command.shortcuts );
        ::fromScriptValueDefined( value.property("globalShortcuts"), scriptable, &command.globalShortcuts );
        ::fromScriptValueDefined( value.property("tab"), scriptable, &command.tab );
        ::fromScriptValueDefined( value.property("outputTab"), scriptable, &command.outputTab );

        return command;
    }
};

template <>
struct ScriptValueFactory<QVariant> {
    static QJSValue toScriptValue(const QVariant &variant, const Scriptable *scriptable)
    {
        if ( !variant.isValid() )
            return QJSValue(QJSValue::UndefinedValue);

        if (variant.type() == QVariant::Bool)
            return ::toScriptValue(variant.toBool(), scriptable);

        if (variant.type() == QVariant::ByteArray)
            return ::toScriptValue(variant.toByteArray(), scriptable);

        if (variant.type() == QVariant::String)
            return ::toScriptValue(variant.toString(), scriptable);

        if (variant.type() == QVariant::RegExp)
            return ::toScriptValue(variant.toRegExp(), scriptable);

        if (variant.canConvert<QVariantList>())
            return ::toScriptValue(variant.value<QVariantList>(), scriptable);

        if (variant.canConvert<QVariantMap>())
            return ::toScriptValue(variant.value<QVariantMap>(), scriptable);

        return scriptable->engine()->toScriptValue(variant);
    }

    static QVariant fromScriptValue(const QJSValue &value, const Scriptable *scriptable)
    {
        auto bytes = getByteArray(value, scriptable);
        if (bytes)
            return QVariant(*bytes);

        auto file = getFile(value, scriptable);
        if (file)
            return QVariant::fromValue(file);

        if (value.isArray())
            return ScriptValueFactory<QVariantList>::fromScriptValue(value, scriptable);

        if (value.isObject())
            return ScriptValueFactory<QVariantMap>::fromScriptValue(value, scriptable);

        const auto variant = toVariant(value);
        Q_ASSERT(value.isUndefined() || value.isNull() || variant.isValid());
        return variant;
    }
};

QByteArray serializeScriptValue(const QJSValue &value, const Scriptable *scriptable)
{
    QByteArray data;

    if ( value.isArray() ) {
        const quint32 len = value.property("length").toUInt();
        for (quint32 i = 0; i < len; ++i)
            data += serializeScriptValue(value.property(i), scriptable);
    } else {
        const auto bytes = getByteArray(value, scriptable);
        if (bytes)
            data = *bytes;
        else if ( !value.isUndefined() )
            data = value.toString().toUtf8() + '\n';
    }

    return data;
}

QString createScriptErrorMessage(const QString &text)
{
    return "ScriptError: " + text;
}

void logScriptError(const QString &text)
{
    log( createScriptErrorMessage(text), LogNote );
}

QString messageCodeToString(int code)
{
    switch (code) {
    case CommandArguments:
        return "CommandArguments";
    case CommandReadInputReply:
        return "CommandReadInputReply";
    default:
        return QString("Unknown(%1)").arg(code);
    }
}

} // namespace

Scriptable::Scriptable(QJSEngine *engine, ScriptableProxy *proxy)
    : m_engine(engine)
    , m_proxy(proxy)
    , m_self( m_engine->newQObject(this) )
{
    QJSValue globalObject = m_engine->globalObject();

    QJSValueIterator it(m_self);
    while (it.hasNext()) {
        it.next();
        globalObject.setProperty( it.name(), it.value() );
    }

    globalObject.setProperty("mimeText", mimeText);
    globalObject.setProperty("mimeHtml", mimeHtml);
    globalObject.setProperty("mimeUriList", mimeUriList);
    globalObject.setProperty("mimeWindowTitle", mimeWindowTitle);
    globalObject.setProperty("mimeItems", mimeItems);
    globalObject.setProperty("mimeItemNotes", mimeItemNotes);
    globalObject.setProperty("mimeOwner", mimeOwner);
    globalObject.setProperty("mimeClipboardMode", mimeClipboardMode);
    globalObject.setProperty("mimeCurrentTab", mimeCurrentTab);
    globalObject.setProperty("mimeSelectedItems", mimeSelectedItems);
    globalObject.setProperty("mimeCurrentItem", mimeCurrentItem);
    globalObject.setProperty("mimeHidden", mimeHidden);
    globalObject.setProperty("mimeShortcut", mimeShortcut);
    globalObject.setProperty("mimeColor", mimeColor);
    globalObject.setProperty("mimeOutputTab", mimeOutputTab);
    globalObject.setProperty("mimeSyncToClipboard", mimeSyncToClipboard);
    globalObject.setProperty("mimeSyncToSelection", mimeSyncToSelection);

    const auto byteArrayMetaObject = m_engine->newQMetaObject<ScriptableByteArray>();
    globalObject.setProperty("ByteArray", byteArrayMetaObject);

    auto dirMetaObject = m_engine->newQMetaObject<ScriptableDir>();
    dirMetaObject.setProperty("homePath", QDir::homePath());
    globalObject.setProperty("Dir", dirMetaObject);

    const auto fileMetaObject = m_engine->newQMetaObject<ScriptableFile>();
    globalObject.setProperty("File", fileMetaObject);

    const auto temporaryFileMetaObject = m_engine->newQMetaObject<ScriptableTemporaryFile>();
    globalObject.setProperty("TemporaryFile", temporaryFileMetaObject);

    setInputSeparator("\n");
}

QJSValue Scriptable::arguments() const
{
    return m_engine->evaluate("arguments");
}

int Scriptable::argumentCount() const
{
    return arguments().property("length").toInt();
}

QJSValue Scriptable::argument(int index) const
{
    return arguments().property( static_cast<quint32>(index) );
}

QString Scriptable::arg(int i, const QString &defaultValue)
{
    return i < argumentCount() ? toString(argument(i), this) : defaultValue;
}

QJSValue Scriptable::throwError(const QString &errorMessage)
{
    m_engine->globalObject().setProperty("_copyqExceptionText", errorMessage);
    return m_engine->evaluate("throw new Error(_copyqExceptionText)");
}

QJSValue Scriptable::newByteArray(const QByteArray &bytes)
{
    return ::newByteArray(bytes, this);
}

QByteArray Scriptable::fromString(const QString &value) const
{
  QByteArray bytes = value.toUtf8();
#ifdef COPYQ_OS_WIN
  bytes.replace('\n', "\r\n");
#endif
  return bytes;
}

QVariant Scriptable::toVariant(const QJSValue &value)
{
    return fromScriptValue<QVariant>(value, this);
}

bool Scriptable::toInt(const QJSValue &value, int *number) const
{
    bool ok;
    *number = toString(value, this).toInt(&ok);
    return ok;
}

QVariantMap Scriptable::toDataMap(const QJSValue &value) const
{
    if ( value.isVariant() )
        return value.toVariant().toMap();

    QVariantMap dataMap;

    QJSValueIterator it(value);
    while (it.hasNext()) {
        it.next();
        dataMap.insert( it.name(), makeByteArray(it.value()) );
    }

    return dataMap;
}

QJSValue Scriptable::fromDataMap(const QVariantMap &dataMap) const
{
    return toScriptValue(dataMap, this);
}

QByteArray Scriptable::makeByteArray(const QJSValue &value) const
{
    const QByteArray *data = getByteArray(value, this);
    return data ? *data : fromString(value.toString());
}

bool Scriptable::toItemData(const QJSValue &value, const QString &mime, QVariantMap *data) const
{
    if (mime == mimeItems) {
        const QByteArray *itemData = getByteArray(value, this);
        if (!itemData)
            return false;

        return deserializeData(data, *itemData);
    }

    const auto bytes = *getByteArray(value, this);
    if ( value.isUndefined() )
        data->insert( mime, QVariant() );
    else if (!mime.startsWith("text/") && bytes != nullptr)
        data->insert( mime, bytes );
    else
        data->insert( mime, toString(value, this).toUtf8() );

    return true;
}

QString Scriptable::getFileName(const QString &fileName) const
{
    return QDir(getCurrentPath()).filePath(fileName);
}

QJSValue Scriptable::version()
{
    m_skipArguments = 0;
    return tr(programName) + " " COPYQ_VERSION " (hluk@email.cz)\n"
            + tr("Built with: ")
            + "Qt " + QT_VERSION_STR
            + '\n';
}

QJSValue Scriptable::help()
{
    m_skipArguments = -1;

    QString helpString;

    if ( argumentCount() == 0 ) {
        helpString.append(helpHead() + "\n");

        for (const auto &hlp : commandHelp())
            helpString.append(hlp.toString());

        helpString.append("\n" + helpTail() + "\n\n" + tr(programName)
            + " " + COPYQ_VERSION + " (hluk@email.cz)\n");
    } else {
        for (int i = 0; i < argumentCount(); ++i) {
            const QString &cmd = toString(argument(i), this);
            for (const auto &helpItem : commandHelp()) {
                if ( helpItem.cmd.contains(cmd) )
                    helpString.append(helpItem.toString());
            }
        }

        if ( helpString.isEmpty() )
            return throwError( tr("Command not found!") );
    }

    return helpString;
}

void Scriptable::show()
{
    m_skipArguments = 1;

    if ( argumentCount() == 0 )
        m_proxy->showWindow();
    else
        m_proxy->showBrowser( toString(argument(0), this) );
}

void Scriptable::showAt()
{
    QRect rect(-1, -1, 0, 0);
    int n;
    int i = 0;
    if ( toInt(argument(i), &n) ) {
        rect.setX(n);
        ++i;

        if ( toInt(argument(i), &n) ) {
            rect.setY(n);
            ++i;

            if ( toInt(argument(i), &n) ) {
                rect.setWidth(n);
                ++i;

                if ( toInt(argument(i), &n) ) {
                    rect.setHeight(n);
                    ++i;
                }
            }
        }
    }

    m_skipArguments = i;

    const auto tabName = arg(i++);
    if ( tabName.isEmpty() )
        m_proxy->showWindowAt(rect);
    else
        m_proxy->showBrowserAt(tabName, rect);
}

void Scriptable::hide()
{
    m_skipArguments = 0;
    m_proxy->close();
}

QJSValue Scriptable::toggle()
{
    m_skipArguments = 0;
    return m_proxy->toggleVisible();
}

QJSValue Scriptable::menu()
{
    m_skipArguments = 4;

    if (argumentCount() == 0) {
        m_proxy->toggleMenu();
    } else {
        const auto tabName = toString(argument(0), this);

        int maxItemCount = -1;
        if (argumentCount() >= 2) {
            const auto value = argument(1);
            if ( !toInt(value, &maxItemCount) || maxItemCount <= 0 )
                return throwError("Argument maxItemCount must be positive number");
        }

        int x = -1;
        int y = -1;
        if (argumentCount() >= 3) {
            const auto xValue = argument(2);
            const auto yValue = argument(3);
            if ( !toInt(xValue, &x) || x < 0 || !toInt(yValue, &y) || y < 0 )
                return throwError("Coordinates must be positive numbers");
        }

        m_proxy->toggleMenu( tabName, maxItemCount, QPoint(x, y) );
    }

    return QJSValue();
}

void Scriptable::exit()
{
    m_skipArguments = 0;
    QByteArray message = fromString( tr("Terminating server.\n") );
    emit sendMessage(message, CommandPrint);
    m_proxy->exit();
}

void Scriptable::disable()
{
    m_skipArguments = 0;
    m_proxy->disableMonitoring(true);
}

void Scriptable::enable()
{
    m_skipArguments = 0;
    m_proxy->disableMonitoring(false);
}

QJSValue Scriptable::monitoring()
{
    m_skipArguments = 0;
    return m_proxy->isMonitoringEnabled();
}

QJSValue Scriptable::visible()
{
    m_skipArguments = 0;
    return m_proxy->isMainWindowVisible();
}

QJSValue Scriptable::focused()
{
    m_skipArguments = 0;
    return m_proxy->isMainWindowFocused();
}

void Scriptable::filter()
{
    m_skipArguments = 1;
    m_proxy->filter(arg(0));
}

void Scriptable::ignore()
{
    m_skipArguments = 0;
    m_proxy->abortAutomaticCommands();
}

QJSValue Scriptable::clipboard()
{
    m_skipArguments = 1;
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime) );
}

QJSValue Scriptable::selection()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime, QClipboard::Selection) );
#else
    return QJSValue();
#endif
}

QJSValue Scriptable::hasClipboardFormat()
{
    m_skipArguments = 1;
    const QString &mime = arg(0);
    return m_proxy->hasClipboardFormat(mime, QClipboard::Clipboard);
}

QJSValue Scriptable::hasSelectionFormat()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0);
    return m_proxy->hasClipboardFormat(mime, QClipboard::Selection);
#else
    return false;
#endif
}

QJSValue Scriptable::isClipboard()
{
    return !m_data.keys().contains(mimeClipboardMode);
}

QJSValue Scriptable::copy()
{
    m_skipArguments = -1;
    return copy(QClipboard::Clipboard);
}

QJSValue Scriptable::copySelection()
{
    m_skipArguments = -1;
#ifdef HAS_MOUSE_SELECTIONS
    return copy(QClipboard::Selection);
#else
    return QJSValue();
#endif
}

void Scriptable::paste()
{
    m_skipArguments = 0;
    m_proxy->pasteToCurrentWindow();
}

QJSValue Scriptable::tab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    if ( name.isNull() )
        return toScriptValue( m_proxy->tabs(), this );

    m_proxy->setTab(name);
    return QJSValue();
}

QJSValue Scriptable::removeTab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    const QString error = m_proxy->removeTab(name);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

QJSValue Scriptable::renameTab()
{
    m_skipArguments = 2;
    const QString &name = arg(0);
    const QString &newName = arg(1);
    const QString error = m_proxy->renameTab(newName, name);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

QJSValue Scriptable::tabIcon()
{
    m_skipArguments = 2;

    if (argumentCount() == 1)
        return m_proxy->tabIcon(arg(0));

    if (argumentCount() >= 2)
        m_proxy->setTabIcon(arg(0), arg(1));
    else
        return throwError(argumentError());

    return QJSValue();
}

QJSValue Scriptable::length()
{
    m_skipArguments = 0;
    return m_proxy->browserLength();
}

void Scriptable::select()
{
    m_skipArguments = 1;
    QJSValue value = argument(0);
    int row;
    if ( toInt(value, &row) )
        m_proxy->browserMoveToClipboard(row);
}

QJSValue Scriptable::next()
{
    m_skipArguments = 0;
    return nextToClipboard(1);
}

QJSValue Scriptable::previous()
{
    m_skipArguments = 0;
    return nextToClipboard(-1);
}

QJSValue Scriptable::add()
{
    m_skipArguments = -1;

    QStringList texts;

    for (int i = 0; i < argumentCount(); ++i)
        texts.append( toString(argument(i), this) );

    const auto error = m_proxy->browserAdd(texts);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

QJSValue Scriptable::insert()
{
    m_skipArguments = 2;

    int row;
    if ( !toInt(argument(0), &row) )
        return throwError(argumentError());

    QJSValue value = argument(1);
    QVariantMap data;
    setTextData( &data, toString(value, this) );
    const auto error = m_proxy->browserAdd(data, row);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

QJSValue Scriptable::remove()
{
    QList<int> rows = getRows();
    m_skipArguments = rows.size();

    if ( rows.empty() )
        rows.append(0);

    const auto error = m_proxy->browserRemoveRows(rows);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

void Scriptable::edit()
{
    m_skipArguments = -1;

    QJSValue value;
    QString text;
    int row = -1;

    const int len = argumentCount();
    const auto separator = toString( inputSeparator(), this );
    for ( int i = 0; i < len; ++i ) {
        value = argument(i);
        if (i > 0)
            text.append(separator);
        if ( toInt(value, &row) ) {
            const QByteArray bytes = row >= 0 ? m_proxy->browserItemData(row, mimeText)
                                              : m_proxy->getClipboardData(mimeText);
            text.append( getTextData(bytes) );
        } else {
            text.append( toString(value, this) );
        }
    }

    bool changeClipboard = row < 0;

    if ( !m_proxy->browserOpenEditor(fromString(text), changeClipboard) ) {
        m_proxy->showBrowser();
        if (len == 1 && row >= 0) {
            m_proxy->browserSetCurrent(row);
            m_proxy->browserEditRow(row);
        } else {
            m_proxy->browserEditNew(text, changeClipboard);
        }
    }
}

QJSValue Scriptable::read()
{
    m_skipArguments = -1;

    QByteArray result;
    QString mime(mimeText);
    QJSValue value;

    bool used = false;
    const auto separator = toByteArray( inputSeparator(), this );
    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, &row) ) {
            if (used)
                result.append(separator);
            used = true;
            result.append( row >= 0 ? m_proxy->browserItemData(row, mime)
                                    : m_proxy->getClipboardData(mime) );
        } else {
            mime = toString(value, this);
        }
    }

    if (!used)
        result.append( m_proxy->getClipboardData(mime) );

    return newByteArray(result);
}

void Scriptable::write()
{
    m_skipArguments = -1;
    changeItem(true);
}

void Scriptable::change()
{
    m_skipArguments = -1;
    changeItem(false);
}

void Scriptable::separator()
{
    setInputSeparator( toString(argument(0), this) );
    m_skipArguments = 1;
}

void Scriptable::action()
{
    QString text;
    bool anyRows = false;
    int i;
    QJSValue value;

    const auto separator = toString( inputSeparator(), this );
    for ( i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if (!toInt(value, &row))
            break;
        if (anyRows)
            text.append(separator);
        else
            anyRows = true;
        text.append( getTextData(m_proxy->browserItemData(row, mimeText)) );
    }

    m_skipArguments = i + 2;

    if (!anyRows) {
        text = getTextData( m_proxy->getClipboardData(mimeText) );
    }

    const QVariantMap data = createDataMap(mimeText, text);

    if (i < argumentCount()) {
        Command command;
        command.cmd = toString(value, this);
        command.output = mimeText;
        command.input = mimeText;
        command.wait = false;
        command.outputTab = m_proxy->tab();
        command.sep = ((i + 1) < argumentCount()) ? toString( argument(i + 1), this )
                                                  : QString('\n');
        m_proxy->action(data, command);
    } else {
        m_proxy->openActionDialog(data);
    }
}

void Scriptable::popup()
{
    m_skipArguments = 3;

    const QString title = arg(0);
    const QString message = arg(1);
    const auto icon = QString(QChar(IconInfoSign));
    int msec;
    if ( !toInt(argument(2), &msec) )
        msec = 8000;
    m_proxy->showMessage(title, message, icon, msec);
}

QJSValue Scriptable::notification()
{
    m_skipArguments = -1;

    QString title;
    QString message;
    int msec = -1;
    auto icon = QString(QChar(IconInfoSign));
    QString notificationId;
    NotificationButtons buttons;

    for ( int i = 0; i < argumentCount(); ++i ) {
        const auto name = arg(i++);
        if ( name == QString(".title") ) {
            title = arg(i);
        } else if ( name == QString(".message") ) {
            message = arg(i);
        } else if ( name == QString(".time") ) {
            if ( !toInt(argument(i), &msec) )
                return throwError("Expected number after .time argument");
        } else if ( name == QString(".id") ) {
            notificationId = arg(i);
        } else if ( name == QString(".icon") ) {
            icon = arg(i);
        } else if ( name == QString(".button") ) {
            NotificationButton button;
            button.name = arg(i);
            button.script = arg(++i);
            button.data = makeByteArray( argument(++i) );
            buttons.append(button);
        } else {
            return throwError("Unknown argument: " + name);
        }
    }

    m_proxy->showMessage(title, message, icon, msec, notificationId, buttons);
    return QJSValue();
}

QJSValue Scriptable::exportTab()
{
    m_skipArguments = 1;

    const QString &fileName = arg(0);
    if ( fileName.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->saveTab(getFileName(fileName)) )
        return throwError( tr("Cannot save to file \"%1\"!").arg(fileName) );

    return QJSValue();
}

QJSValue Scriptable::importTab()
{
    m_skipArguments = 1;

    const QString &fileName = arg(0);
    if ( fileName.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->loadTab(getFileName(fileName)) ) {
        return throwError(
            tr("Cannot import file \"%1\"!").arg(fileName) );
    }

    return QJSValue();
}

QJSValue Scriptable::importData()
{
    m_skipArguments = 1;

    const auto fileName = arg(0);
    return m_proxy->importData(fileName);
}

QJSValue Scriptable::exportData()
{
    m_skipArguments = 1;

    const auto fileName = arg(0);
    return m_proxy->exportData(fileName);
}

QJSValue Scriptable::config()
{
    m_skipArguments = -1;

    QStringList nameValueInput;
    nameValueInput.reserve( argumentCount() );

    for (int i = 0; i < argumentCount(); ++i)
        nameValueInput.append( arg(i) );

    const auto result = m_proxy->config(nameValueInput);
    if ( result.type() == QVariant::String )
        return result.toString();

    if ( result.type() == QVariant::StringList ) {
        QString errors;
        const auto unknownOptions = result.toStringList();
        for (const auto &name : unknownOptions) {
            if ( !errors.isEmpty() )
                errors.append('\n');
            errors.append( tr("Invalid option \"%1\"!").arg(name) );
        }

        return throwError(errors);
    }

    const auto nameValue = result.toMap();
    if ( nameValue.size() == 1 )
        return nameValue.constBegin().value().toString();

    QStringList output;
    for (auto it = nameValue.constBegin(); it != nameValue.constEnd(); ++it) {
        const auto name = it.key();
        const auto value = it.value();
        output.append( name + "=" + value.toString() );
    }

    return toScriptValue(output, this);
}

QJSValue Scriptable::toggleConfig()
{
    m_skipArguments = 1;
    const auto optionName = arg(0);
    if ( optionName.isEmpty() )
        return throwError(argumentError());

    const auto result = m_proxy->toggleConfig(optionName);
    if ( result.type() != QVariant::Bool )
        return throwError( QString("Invalid boolean option \"%1\"!").arg(optionName) );

    return result.toBool();
}

QJSValue Scriptable::info()
{
    m_skipArguments = 1;

    using InfoMap = QMap<QString, QString>;
    InfoMap info;
    info.insert("config", QSettings().fileName());
    info.insert("exe", QCoreApplication::applicationFilePath());
    info.insert("log", logFileName());

    info.insert("plugins",
#ifdef COPYQ_PLUGIN_PREFIX
                COPYQ_PLUGIN_PREFIX
#else
                m_proxy ? m_proxy->pluginsPath() : pluginsPath()
#endif
                );

    info.insert("themes",
#ifdef COPYQ_THEME_PREFIX
                COPYQ_THEME_PREFIX
#else
                m_proxy ? m_proxy->themesPath() : themesPath()
#endif
                );

    info.insert("translations",
#ifdef COPYQ_TRANSLATION_PREFIX
                COPYQ_TRANSLATION_PREFIX
#else
                m_proxy ? m_proxy->translationsPath() : translationsPath()
#endif
                );

    info.insert("themes(custom)", qgetenv("COPYQ_THEME_PREFIX"));
    info.insert("translations(custom)", qgetenv("COPYQ_TRANSLATION_PREFIX"));

    info.insert("icons",
#ifdef COPYQ_ICON_PREFIX
                COPYQ_ICON_PREFIX
#else
                QString()
#endif
                );

    info.insert("desktop",
#ifdef COPYQ_DESKTOP_PREFIX
                COPYQ_DESKTOP_PREFIX
#else
                QString()
#endif
                );

    info.insert("has-mouse-selection",
#ifdef HAS_MOUSE_SELECTIONS
                "1"
#else
                "0"
#endif
                );

    info.insert("has-global-shortcuts",
#ifdef NO_GLOBAL_SHORTCUTS
                "0"
#else
                "1"
#endif
                );

    info.insert("platform",
#if defined(Q_OS_WIN)
                "Windows"
#elif defined(Q_OS_MAC)
                "OS X"
#elif defined(Q_OS_LINUX)
                "Linux"
#elif defined(Q_OS_UNIX)
                "Unix"
#else
                "?"
#endif
#ifdef COPYQ_WS_X11
                "/X11"
#endif
                );

    const QString name = arg(0);
    if (!name.isEmpty())
        return info.value(name);

    QString result;
    for (auto it = info.constBegin(); it != info.constEnd(); ++it)
        result.append( QString("%1: %2\n").arg(it.key(), it.value()) );
    result.chop(1);

    return result;
}

QJSValue Scriptable::eval()
{
    const auto script = arg(0);
    const auto result = eval(script);
    m_skipArguments = -1;
    return result;
}

QJSValue Scriptable::source()
{
    const auto scriptFilePath = arg(0);

    QByteArray script;
    {
        QFile scriptFile( getFileName(scriptFilePath) );
        if ( !scriptFile.open(QIODevice::ReadOnly) ) {
            return throwError(
                    QString("Failed to open \"%1\": %2")
                        .arg(scriptFilePath, scriptFile.errorString()) );
        }

        script = scriptFile.readAll();
    }

    const auto result = eval(script, scriptFilePath);
    m_skipArguments = 1;
    return result;
}

QJSValue Scriptable::currentPath()
{
    m_skipArguments = 1;

    if (argumentCount() > 0)
        setCurrentPath(arg(0));

    return getCurrentPath();
}

QJSValue Scriptable::str(const QJSValue &value)
{
    m_skipArguments = 1;
    return toString(value, this);
}

QJSValue Scriptable::input()
{
    m_skipArguments = 0;

    if ( !getByteArray(m_input, this) ) {
        emit sendMessage(QByteArray(), CommandReadInput);
        QEventLoop loop;
        connect(this, SIGNAL(finished()), &loop, SLOT(quit()));
        connect(this, SIGNAL(dataReceived()), &loop, SLOT(quit()));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    return m_input;
}

QJSValue Scriptable::toUnicode()
{
    m_skipArguments = 2;

    const auto bytes = makeByteArray(argument(0));

    if (argumentCount() >= 2) {
        const auto codec = codecFromNameOrNull(argument(1));
        if (codec == nullptr)
            return availableCodecNamesError();

        return codec->toUnicode(bytes);
    }

    if (argumentCount() >= 1) {
        const auto codec = QTextCodec::codecForUtfText(bytes, nullptr);
        if (!codec)
            return throwError("Failed to detect encoding");
        return codec->toUnicode(bytes);
    }

    return throwError(argumentError());
}

QJSValue Scriptable::fromUnicode()
{
    m_skipArguments = 2;

    if (argumentCount() < 2)
        return throwError(argumentError());

    const auto codec = codecFromNameOrNull(argument(1));
    if (codec == nullptr)
        return availableCodecNamesError();

    const auto text = arg(0);
    return newByteArray( codec->fromUnicode(text) );
}

QJSValue Scriptable::dataFormats()
{
    m_skipArguments = 0;
    return toScriptValue( m_data.keys(), this );
}

QJSValue Scriptable::data(const QJSValue &value)
{
    m_skipArguments = 1;
    return newByteArray( m_data.value(toString(value, this)).toByteArray() );
}

QJSValue Scriptable::setData()
{
    m_skipArguments = 2;

    const QString mime = arg(0);
    if ( !toItemData(argument(1), mime, &m_data) )
        return false;

    if ( m_data.value(mimeSelectedItems).isValid() )
        m_proxy->setSelectedItemsData(mime, m_data.value(mime));

    return true;
}

QJSValue Scriptable::removeData()
{
    m_skipArguments = 1;

    const QString mime = arg(0);
    m_data.remove(mime);
    m_proxy->setSelectedItemsData(mime, QVariant());
    return true;
}

void Scriptable::print(const QJSValue &value)
{
    m_skipArguments = 1;
    emit sendMessage(makeByteArray(value), CommandPrint);
}

QJSValue Scriptable::abort()
{
    m_skipArguments = 0;

    if (m_engine) {
        m_aborted = true;
        return m_engine->evaluate("throw Error()");
    }

    return QJSValue();
}

void Scriptable::fail()
{
    m_skipArguments = 0;
    emit sendMessage("", CommandError);
}

#ifdef HAS_TESTS
QJSValue Scriptable::keys()
{
    m_skipArguments = -1;

    bool ok;

    // Wait interval after shortcut pressed or text typed.
    const auto waitValue = qgetenv("COPYQ_TESTS_KEYS_WAIT");
    int wait = waitValue.toInt(&ok);
    if (!ok)
        wait = 0;

    // Delay while typing.
    const auto delayValue = qgetenv("COPYQ_TESTS_KEY_DELAY");
    int delay = delayValue.toInt(&ok);
    if (!ok)
        delay = 0;

    for (int i = 0; i < argumentCount(); ++i) {
        const QString keys = toString(argument(i), this);

        waitFor(wait);
        m_proxy->sendKeys(keys, delay);

        // Make sure all keys are send (shortcuts are postponed because they can be blocked by modal windows).
        while ( !m_proxy->keysSent() ) {
            QCoreApplication::processEvents();
            if (!m_connected)
                return throwError("Disconnected");
        }
    }

    return QJSValue();
}

QJSValue Scriptable::testSelected()
{
    m_skipArguments = 0;
    return m_proxy->testSelected();
}

void Scriptable::resetTestSession()
{
    m_skipArguments = 1;
    m_proxy->resetTestSession( arg(0) );
}

void Scriptable::flush()
{
    m_skipArguments = 1;
    log("flush ID: " + arg(0), LogAlways);
}
#else // HAS_TESTS
void Scriptable::keys()
{
    m_skipArguments = -1;
}

QJSValue Scriptable::testSelected()
{
    m_skipArguments = 0;
    return QJSValue();
}

void Scriptable::resetTestSession()
{
    m_skipArguments = 1;
}

void Scriptable::flush()
{
    m_skipArguments = 1;
}
#endif // HAS_TESTS

void Scriptable::setCurrentTab()
{
    m_skipArguments = 1;
    const QString tabName = arg(0);
    m_proxy->setCurrentTab(tabName);
}

QJSValue Scriptable::selectItems()
{
    QList<int> rows = getRows();
    m_skipArguments = rows.size();
    return m_proxy->selectItems(rows);
}

QJSValue Scriptable::selectedTab()
{
    m_skipArguments = 0;
    return m_data.value(mimeCurrentTab).toString();
}

QJSValue Scriptable::selectedItems()
{
    m_skipArguments = 0;
    return toScriptValue( m_proxy->selectedItems(), this );
}

QJSValue Scriptable::currentItem()
{
    m_skipArguments = 0;
    return m_proxy->currentItem();
}

QJSValue Scriptable::selectedItemData()
{
    int selectedIndex;
    if ( !toInt(argument(0), &selectedIndex) )
        return throwError(argumentError());

    return toScriptValue( m_proxy->selectedItemData(selectedIndex), this );
}

QJSValue Scriptable::setSelectedItemData()
{
    int selectedIndex;
    if ( !toInt(argument(0), &selectedIndex) )
        return throwError(argumentError());

    const auto data = toDataMap( argument(1) );
    return toScriptValue( m_proxy->setSelectedItemData(selectedIndex, data), this );
}

QJSValue Scriptable::selectedItemsData()
{
    return toScriptValue( m_proxy->selectedItemsData(), this );
}

void Scriptable::setSelectedItemsData()
{
    m_skipArguments = 1;
    const auto dataList = fromScriptValue<QList<QVariantMap>>( argument(0), this );
    m_proxy->setSelectedItemsData(dataList);
}

QJSValue Scriptable::escapeHtml()
{
    m_skipArguments = 1;
    return ::escapeHtml( toString(argument(0), this) );
}

QJSValue Scriptable::unpack()
{
    m_skipArguments = 1;

    QVariantMap data;

    if ( !toItemData(argument(0), mimeItems, &data) )
        return throwError(argumentError());

    return toScriptValue(data, this);
}

QJSValue Scriptable::pack()
{
    m_skipArguments = 1;
    QVariantMap data = toDataMap( argument(0) );
    return newByteArray(serializeData(data));
}

QJSValue Scriptable::getItem()
{
    m_skipArguments = 1;

    int row;
    if ( !toInt(argument(0), &row) )
        return throwError(argumentError());

    return toScriptValue( m_proxy->browserItemData(row), this );
}

QJSValue Scriptable::setItem()
{
    m_skipArguments = 2;

    int row;
    if ( !toInt(argument(0), &row) )
        return throwError(argumentError());

    QVariantMap data = toDataMap( argument(1) );
    const auto error = m_proxy->browserAdd(data, row);
    if ( !error.isEmpty() )
        return throwError(error);

    return QJSValue();
}

QJSValue Scriptable::toBase64()
{
    m_skipArguments = 1;
    return QString::fromLatin1(makeByteArray(argument(0)).toBase64());
}

QJSValue Scriptable::fromBase64()
{
    m_skipArguments = 1;
    return newByteArray(QByteArray::fromBase64(makeByteArray(argument(0))));
}

QJSValue Scriptable::open()
{
    m_skipArguments = -1;

    for ( int i = 0; i < argumentCount(); ++i ) {
        if ( !QDesktopServices::openUrl(QUrl(toString(argument(i), this))) )
            return false;
    }

    return true;
}

QJSValue Scriptable::execute()
{
    m_skipArguments = -1;

    m_executeStdoutCallback = QJSValue();

    // Pass all arguments until null to command. The rest will be sent to stdin.
    QStringList args;
    int i = 0;
    for ( ; i < argumentCount(); ++i ) {
        const auto arg = argument(i);
        if (arg.isNull())
            break;

        if ( arg.isCallable() )
            m_executeStdoutCallback = arg;
        else
            args.append( toString(arg, this) );
    }

    Action action;
    for ( ++i ; i < argumentCount(); ++i ) {
        const auto arg = argument(i);
        if ( arg.isCallable() )
            m_executeStdoutCallback = arg;
        else
            action.setInput( action.input() + makeByteArray(arg) );
    }

    if ( m_executeStdoutCallback.isCallable() ) {
        action.setItemSeparator(QRegExp("\n"));
        action.setOutputFormat(mimeText);
        connect( &action, SIGNAL(newItems(QStringList, QString, QString)),
                 this, SLOT(onExecuteOutput(QStringList)) );
    } else {
        action.setOutputFormat("DATA");
    }

    action.setCommand(args);
    action.setWorkingDirectory( getCurrentPath() );
    action.start();

    if ( !action.waitForStarted(5000) )
        return QJSValue();

    while ( !action.waitForFinished(5000) && m_connected ) {}

    if ( action.isRunning() && !action.waitForFinished(5000) ) {
        action.terminate();
        return QJSValue();
    }

    if ( action.actionFailed() )
        return QJSValue();

    QJSValue actionResult = m_engine->newObject();
    actionResult.setProperty( "stdout", newByteArray(action.outputData()) );
    actionResult.setProperty( "stderr", action.errorOutput() );
    actionResult.setProperty( "exit_code", action.exitCode() );

    return actionResult;
}

QJSValue Scriptable::currentWindowTitle()
{
    m_skipArguments = 0;
    return m_proxy->currentWindowTitle();
}

QJSValue Scriptable::dialog()
{
    m_skipArguments = -1;

    NamedValueList values;
    values.reserve(argumentCount() / 2);

    for ( int i = 0; i < argumentCount(); i += 2 ) {
        const QString key = arg(i);
        const QJSValue value = argument(i + 1);
        values.append( NamedValue(key, toVariant(value)) );
    }

    values = m_proxy->inputDialog(values);

    if (values.isEmpty())
        return QJSValue();

    if (values.size() == 1)
        return toScriptValue( values.first().value, this );

    QJSValue result = m_engine->newObject();

    for (const auto &value : values)
        result.setProperty( value.name, toScriptValue(value.value, this) );

    return result;
}

QJSValue Scriptable::settings()
{
    m_skipArguments = 2;

    if (argumentCount() == 2) {
        const QString key = arg(0);
        const QJSValue value = argument(1);
        const QVariant saveValue = toVariant(value);
        m_proxy->setUserValue(key, saveValue);

        return QJSValue();
    }

    QSettings settings;
    settings.beginGroup("script");

    if (argumentCount() == 1) {
        const auto value = settings.value(arg(0));
        return toScriptValue(value, this);
    }

    return toScriptValue(settings.allKeys(), this);
}

QJSValue Scriptable::dateString()
{
    m_skipArguments = 1;
    const QDateTime dateTime = QDateTime::currentDateTime();
    return dateTime.toString(arg(0));
}

void Scriptable::updateFirst()
{
    m_skipArguments = 0;
    m_proxy->updateFirstItem(m_data);
}

void Scriptable::updateTitle()
{
    m_skipArguments = 1;
    if (argumentCount() == 0)
        m_proxy->updateTitle(m_data);
    else
        m_proxy->updateTitle(QVariantMap());
}

QJSValue Scriptable::commands()
{
    return toScriptValue( m_proxy->commands(), this );
}

void Scriptable::setCommands()
{
    const auto commands = fromScriptValue<QList<Command>>(argument(0), this);
    m_proxy->setCommands(commands);
}

void Scriptable::addCommands()
{
    const auto commands = fromScriptValue<QList<Command>>(argument(0), this);
    m_proxy->addCommands(commands);
}

QJSValue Scriptable::importCommands()
{
    m_skipArguments = 1;
    const auto commands = importCommandsFromText(arg(0));
    return toScriptValue(commands, this);
}

QJSValue Scriptable::exportCommands()
{
    m_skipArguments = 1;
    const auto commands = fromScriptValue<QList<Command>>(argument(0), this);

    const auto exportedCommands = ::exportCommands(commands);
    if ( exportedCommands.isEmpty() )
        return throwError("Failed to export commands");

    return exportedCommands;
}

QJSValue Scriptable::networkGet()
{
    /// TODO
    return QJSValue();
    /*
    m_skipArguments = 1;
    const QString url = arg(0);
    return NetworkReply::get(url, this);
    */
}

QJSValue Scriptable::networkPost()
{
    /// TODO
    return QJSValue();
    /*
    m_skipArguments = 2;
    const QString url = arg(0);
    const QByteArray postData = makeByteArray(argument(1));
    return NetworkReply::post(url, postData, this);
    */
}

QJSValue Scriptable::env()
{
    m_skipArguments = 1;
    const QString name = arg(0);
    const QByteArray value = qgetenv(name.toUtf8().constData());
    return newByteArray(value);
}

QJSValue Scriptable::setEnv()
{
    m_skipArguments = 2;
    const QString name = arg(0);
    const QByteArray value = makeByteArray(argument(1));
    return qputenv(name.toUtf8().constData(), value);
}

QJSValue Scriptable::sleep()
{
    m_skipArguments = 1;

    class ThreadSleep : public QThread {
    public:
        static void msleep(unsigned long msec) {
            QThread::msleep(msec);
        }
    };

    int msec;
    if ( !toInt(argument(0), &msec) )
        return throwError(argumentError());

    if (msec > 0)
        ThreadSleep::msleep( static_cast<unsigned long>(msec) );

    return QJSValue();
}

QVariant Scriptable::call(const QString &method, const QVariantList &arguments)
{
    if ( hasUncaughtException() )
        return QVariant();

    m_skipArguments = 2;

    QJSValueList fnArgs;
    for (const auto &argument : arguments)
        fnArgs.append( toScriptValue(argument, this) );

    auto fn = m_engine->globalObject().property(method);
    const auto result = call(&fn, fnArgs);
    // FIXME: What to return if exception is thrown?
    return toVariant(result);
}

QVariantList Scriptable::currentArguments()
{
    m_skipArguments = -1;
    QVariantList arguments;
    for ( int i = 0; i < argumentCount(); ++i )
        arguments.append( toVariant(argument(i)) );
    return arguments;
}

QJSValue Scriptable::screenshot()
{
    return screenshot(false);
}

QJSValue Scriptable::screenshotSelect()
{
    return screenshot(true);
}

QJSValue Scriptable::screenNames()
{
#if QT_VERSION < 0x050000
    return throwError("Screen names are unsupported on version compiled with Qt 4");
#else
    return toScriptValue( m_proxy->screenNames(), this );
#endif
}

QJSValue Scriptable::queryKeyboardModifiers()
{
    const auto modifiers = m_proxy->queryKeyboardModifiers();
    QStringList modifierList;
    if (modifiers.testFlag(Qt::ControlModifier))
        modifierList.append("Ctrl");
    if (modifiers.testFlag(Qt::ShiftModifier))
        modifierList.append("Shift");
    if (modifiers.testFlag(Qt::AltModifier))
        modifierList.append("Alt");
    if (modifiers.testFlag(Qt::MetaModifier))
        modifierList.append("Meta");
    return toScriptValue(modifierList, this);
}

QJSValue Scriptable::iconColor()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconColor();

    const auto color = arg(0);
    if ( !m_proxy->setIconColor(color) )
        return throwError("Invalid color name");

    return QJSValue();
}

void Scriptable::onMessageReceived(const QByteArray &bytes, int messageCode)
{
    COPYQ_LOG( "Message received: " + messageCodeToString(messageCode) );

    if (messageCode == CommandArguments) {
        executeArguments(bytes);
    } else if (messageCode == CommandReadInputReply) {
        m_input = newByteArray(bytes);
        emit dataReceived();
    } else {
        log("Incorrect message code from client", LogError);
    }
}

void Scriptable::onDisconnected()
{
    m_connected = false;
    abort();
    emit finished();
}

void Scriptable::onExecuteOutput(const QStringList &lines)
{
    if ( m_executeStdoutCallback.isCallable() ) {
        const auto arg = toScriptValue(lines, this);
        call( &m_executeStdoutCallback, QJSValueList() << arg );
    }
}

void Scriptable::executeArguments(const QByteArray &bytes)
{
    Arguments args;
    QDataStream stream(bytes);
    stream >> args;
    if ( stream.status() != QDataStream::Ok ) {
        log("Failed to read client arguments", LogError);
        emit sendMessage("Failed to read client arguments", CommandError);
        return;
    }

    if ( hasLogLevel(LogDebug) ) {
        const bool isEval = args.length() == Arguments::Rest + 3
                && args.at(Arguments::Rest) == "eval"
                && args.at(Arguments::Rest + 1) == "--";

        const int skipArgs = isEval ? 2 : 0;
        auto msg = QString("Client-%1:").arg( getTextData(args.at(Arguments::ProcessId)) );
        for (int i = Arguments::Rest + skipArgs; i < args.length(); ++i) {
            const QString indent = isEval
                    ? QString()
                    : (QString::number(i - Arguments::Rest + 1) + " ");
            msg.append( "\n" + indent + getTextData(args.at(i)) );
        }
        COPYQ_LOG(msg);
    }

    const auto currentPath = getTextData(args.at(Arguments::CurrentPath));
    setCurrentPath(currentPath);

    bool hasData;
    const int id = args.at(Arguments::ActionId).toInt(&hasData);
    if (hasData)
        m_data = m_proxy->getActionData(id);
    const auto oldData = m_data;

    m_actionName = getTextData( args.at(Arguments::ActionName) );

    QByteArray response;
    int exitCode;

    if ( args.isEmpty() ) {
        logScriptError("Bad command syntax");
        exitCode = CommandBadSyntax;
    } else {
        /* Special arguments:
             * "-"  read this argument from stdin
             * "--" read all following arguments without control sequences
             */
        QJSValueList fnArgs;
        bool readRaw = false;
        for ( int i = Arguments::Rest; i < args.length(); ++i ) {
            const QByteArray &arg = args.at(i);
            if (!readRaw && arg == "--") {
                readRaw = true;
            } else {
                const QJSValue value = (readRaw || arg != "-")
                        ? newByteArray(arg)
                        : input();
                fnArgs.append(value);
            }
        }

        QString cmd;
        QJSValue result;

        int skipArguments = 0;
        while ( skipArguments < fnArgs.size() && !m_aborted ) {
            if ( result.isCallable() ) {
                m_skipArguments = -1;
                result = call( &result, fnArgs.mid(skipArguments) );
                if ( hasUncaughtException(result) )
                    break;
                if (m_skipArguments == -1)
                    break;
                skipArguments += m_skipArguments;
            } else {
                cmd = toString(fnArgs[skipArguments], this);
                ++skipArguments;

                // FIXME: Thrown exception won't be recognized if it's not instance of Error().
                result = eval(cmd);
                if ( hasUncaughtException(result) )
                    break;
            }
        }

        if ( !m_aborted && !hasUncaughtException() && result.isCallable() )
            result = call( &result, fnArgs.mid(skipArguments) );

        if ( !m_aborted && hasUncaughtException() ) {
            response = createScriptErrorMessage(uncaughtExceptionText()).toUtf8();
            exitCode = CommandException;
        } else {
            response = serializeScriptValue(result, this);
            exitCode = CommandFinished;
        }
    }

    if (exitCode == CommandFinished && hasData && oldData != m_data)
        m_proxy->setActionData(id, m_data);

    // Destroy objects so destructors are run before script finishes
    // (e.g. file writes are flushed or temporary files are automatically removed).
    m_engine->collectGarbage();

    emit sendMessage(response, exitCode);

    COPYQ_LOG("DONE");
}

QJSValue Scriptable::call(QJSValue *function, const QJSValueList &arguments)
{
    auto args = m_engine->newArray(
                arguments.isEmpty() ? 0 : static_cast<uint>(arguments.length()) );
    for ( int i = 0; i < arguments.length(); ++i )
        args.setProperty( static_cast<quint32>(i), arguments[i] );
    m_engine->globalObject().setProperty("arguments", args);
    Q_ASSERT(argumentCount() == arguments.length());
    return function->call(arguments);
}

QJSValue Scriptable::eval(const QString &script, const QString &fileName)
{
    // TODO: Check syntax.
    return m_engine->evaluate(script, fileName);
}

QList<int> Scriptable::getRows() const
{
    QList<int> rows;

    for ( int i = 0; i < argumentCount(); ++i ) {
        int row;
        if ( !toInt(argument(i), &row) )
            break;
        rows.append(row);
    }

    return rows;
}

QJSValue Scriptable::copy(QClipboard::Mode mode)
{
    const int args = argumentCount();
    QVariantMap data;

    if (args == 0) {
        // Reset clipboard first.
        const QString mime = COPYQ_MIME_PREFIX "invalid";
        const QByteArray value = "invalid";
        data.insert(mime, value);
        const auto result = setClipboard(&data, mode);
        if ( result.isError() )
            return result;

        m_proxy->copyFromCurrentWindow();

        // Wait for clipboard to be set.
        for (int i = 0; i < 10; ++i) {
            if ( m_proxy->getClipboardData(mime) != value )
                return true;
            waitFor(5 + i * 25);
        }

        return throwError( tr("Failed to copy!") );
    }

    if (args == 1) {
        QJSValue value = argument(0);
        setTextData( &data, toString(value, this) );
        return setClipboard(&data, mode);
    }

    if (args % 2 == 0) {
        for (int i = 0; i < args; ++i) {
            // MIME
            QString mime = toString(argument(i), this);

            // DATA
            toItemData(argument(++i), mime, &data);
        }

        return setClipboard(&data, mode);
    }

    return throwError(argumentError());
}

QJSValue Scriptable::setClipboard(QVariantMap *data, QClipboard::Mode mode)
{
    const QString mime = COPYQ_MIME_PREFIX "hash";
    data->remove(mime);
    const QByteArray id = QByteArray::number(hash(*data));
    data->insert(mime, id);

    for (int i = 0; i < setClipboardMaxRetries; ++i) {
        m_proxy->setClipboard(*data, mode);

        // Wait for clipboard to be set.
        for (int j = 0; j < 10; ++j) {
            if ( clipboardContains(mode, m_proxy, mime, id) )
                return true;
            waitFor(5 + i * 20 + j * 10);
        }
    }

    return throwError( tr("Failed to set clipboard!") );
}

QJSValue Scriptable::changeItem(bool create)
{
    int row;
    int args = argumentCount();
    int i;

    // [ROW]
    if ( toInt(argument(0), &row) ) {
        if (args < 3 || args % 2 != 1 )
            return throwError(argumentError());
        i = 1;
    } else {
        if (args < 2 || args % 2 != 0 )
            return throwError(argumentError());
        row = 0;
        i = 0;
    }

    QVariantMap data;

    for (; i < args; i += 2) {
        // MIME
        const QString mime = toString(argument(i), this);
        // DATA
        toItemData( argument(i + 1), mime, &data );
    }

    if (create) {
        const auto error = m_proxy->browserAdd(data, row);
        if ( !error.isEmpty() )
            return throwError(error);
    } else {
        m_proxy->browserChange(data, row);
    }

    return QJSValue();
}

QJSValue Scriptable::nextToClipboard(int where)
{
    QVariantMap data = m_proxy->nextItem(where);
    if (data.isEmpty())
        return QJSValue();

    const auto value = setClipboard(&data, QClipboard::Clipboard);
    if (value.isError())
        return value;
#ifdef HAS_MOUSE_SELECTIONS
    return setClipboard(&data, QClipboard::Selection);
#else
    return value;
#endif
}

QJSValue Scriptable::screenshot(bool select)
{
    m_skipArguments = 2;

    const auto format = arg(0, "png");
    const auto screen = arg(1);
    const auto imageData = m_proxy->screenshot(format, screen, select);

#if QT_VERSION < 0x050000
    if ( !screen.isEmpty() )
        return throwError("Screen names are unsupported on version compiled with Qt 4");
#endif

    if ( imageData.isEmpty() ) {
        QString error = "Failed to grab screenshot";
        if ( !screen.isEmpty() ) {
            const auto screenNames = m_proxy->screenNames();
            error.append( " (valid screen names are " + screenNames.join(", ") + ")" );
        }
        return throwError(error);
    }

    return newByteArray(imageData);
}

QJSValue Scriptable::eval(const QString &script)
{
    const int i = script.indexOf('\n');
    const QString name = quoteString( i == -1 ? script : script.mid(0, i) + "..." );
    return eval(script, name);
}

QTextCodec *Scriptable::codecFromNameOrNull(const QJSValue &codecName)
{
    return QTextCodec::codecForName( makeByteArray(codecName) );
}

QJSValue Scriptable::availableCodecNamesError()
{
    QString codecs;
    for (const auto &availableCodecName : QTextCodec::availableCodecs())
        codecs.append( "\n" + QString::fromUtf8(availableCodecName) );
    return throwError("Available codecs are:" + codecs);
}

bool Scriptable::hasUncaughtException(const QJSValue &value)
{
    if ( hasUncaughtException() )
        return true;

    if ( !value.isError() )
        return false;

    m_hasUncaughtException = true;
    m_uncaughtException = value;
    return true;
}

QJSValue Scriptable::inputSeparator() const
{
    return m_engine->globalObject().property("inputSeparator");
}

void Scriptable::setInputSeparator(const QString &separator)
{
    m_engine->globalObject().setProperty("inputSeparator", separator);
}

QString Scriptable::getCurrentPath() const
{
    return toString( m_engine->globalObject().property("currentPath"), this );
}

void Scriptable::setCurrentPath(const QString &path)
{
    m_engine->globalObject().setProperty("currentPath", path);
}

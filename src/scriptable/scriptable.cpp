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
#include "common/command.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/log.h"
#include "common/sleeptimer.h"
#include "common/version.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "item/serialize.h"
#include "scriptable/commandhelp.h"
#include "scriptable/dirclass.h"
#include "scriptable/fileclass.h"
#include "scriptable/scriptableproxy.h"
#include "scriptable/temporaryfileclass.h"
#include "../qt/bytearrayclass.h"
#include "../qxt/qxtglobal.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPoint>
#include <QRegExp>
#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSettings>
#include <QUrl>
#include <QTextCodec>
#include <QThread>

Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

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
        + Scriptable::tr("  - Use dash argument (-) to read data from stdandard input.") + "\n"
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

QByteArray *getByteArray(const QScriptValue &value, const Scriptable *scriptable)
{
    if (value.scriptClass() == scriptable->byteArrayClass())
        return qscriptvalue_cast<QByteArray*>(value.data());
    return nullptr;
}

QFile *getFile(const QScriptValue &value, const Scriptable *scriptable)
{
    if (value.scriptClass() == scriptable->fileClass() || value.scriptClass() == scriptable->temporaryFileClass())
        return qscriptvalue_cast<QFile*>(value.data());
    return nullptr;
}

QString toString(const QScriptValue &value, const Scriptable *scriptable)
{
    QByteArray *bytes = getByteArray(value, scriptable);
    return (bytes == nullptr) ? value.toString() : getTextData(*bytes);
}

QVariant toVariant(const QScriptValue &value)
{
    const auto variant = value.toVariant();
    Q_ASSERT(value.isUndefined() || value.isNull() || variant.isValid());
    return variant;
}

template <typename T>
struct ScriptValueFactory {
    static QScriptValue toScriptValue(const T &value, Scriptable *)
    {
        return QScriptValue(value);
    }

    static T fromScriptValue(const QScriptValue &value, Scriptable *)
    {
        const auto variant = toVariant(value);
        Q_ASSERT( variant.canConvert<T>() );
        return variant.value<T>();
    }
};

template <typename T>
QScriptValue toScriptValue(const T &value, Scriptable *scriptable)
{
    return ScriptValueFactory<T>::toScriptValue(value, scriptable);
}

template <typename T>
T fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
{
    return ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename T>
void fromScriptValueIfValid(const QScriptValue &value, Scriptable *scriptable, T *outputValue)
{
    if (value.isValid())
        *outputValue = ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename T>
struct ScriptValueFactory< QList<T> > {
    static QScriptValue toScriptValue(const QList<T> &list, Scriptable *scriptable)
    {
        QScriptValue array = scriptable->engine()->newArray();
        for ( int i = 0; i < list.size(); ++i ) {
            const auto value = ScriptValueFactory<T>::toScriptValue(list[i], scriptable);
            array.setProperty( static_cast<quint32>(i), value );
        }
        return array;
    }

    static QList<T> fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        if ( !value.isArray() )
            return QList<T>();

        const quint32 length = value.property("length").toUInt32();
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
    static QScriptValue toScriptValue(const QVariantMap &dataMap, Scriptable *scriptable)
    {
        QScriptValue value = scriptable->engine()->newObject();

        for ( const auto &format : dataMap.keys() )
            value.setProperty( format, ::toScriptValue(dataMap[format], scriptable) );

        return value;
    }

    static QVariantMap fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        QVariantMap result;
        QScriptValueIterator it(value);
        while ( it.hasNext() ) {
            it.next();
            const auto itemValue = ::fromScriptValue<QVariant>( it.value(), scriptable );
            result.insert(it.name(), itemValue);
        }
        return result;
    }
};

template <>
struct ScriptValueFactory<QByteArray> {
    static QScriptValue toScriptValue(const QByteArray &bytes, Scriptable *scriptable)
    {
        return scriptable->byteArrayClass()->newInstance(bytes);
    }
};

template <>
struct ScriptValueFactory<QStringList> {
    static QScriptValue toScriptValue(const QStringList &list, Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::toScriptValue(list, scriptable);
    }

    static QStringList fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::fromScriptValue(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QString> {
    static QScriptValue toScriptValue(const QString &text, Scriptable *)
    {
        return QScriptValue(text);
    }

    static QString fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        return toString(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QRegExp> {
    static QScriptValue toScriptValue(const QRegExp &re, Scriptable *scriptable)
    {
        return scriptable->engine()->newRegExp(re);
    }

    static QRegExp fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        return value.isRegExp()
                ? value.toRegExp()
                : QRegExp( toString(value, scriptable) );
    }
};

template <>
struct ScriptValueFactory<Command> {
    static QScriptValue toScriptValue(const Command &command, Scriptable *scriptable)
    {
        QScriptValue value = scriptable->engine()->newObject();

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

    static Command fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
    {
        Command command;

        ::fromScriptValueIfValid( value.property("name"), scriptable, &command.name );
        ::fromScriptValueIfValid( value.property("re"), scriptable, &command.re );
        ::fromScriptValueIfValid( value.property("wndre"), scriptable, &command.wndre );
        ::fromScriptValueIfValid( value.property("matchCmd"), scriptable, &command.matchCmd );
        ::fromScriptValueIfValid( value.property("cmd"), scriptable, &command.cmd );
        ::fromScriptValueIfValid( value.property("sep"), scriptable, &command.sep );
        ::fromScriptValueIfValid( value.property("input"), scriptable, &command.input );
        ::fromScriptValueIfValid( value.property("output"), scriptable, &command.output );
        ::fromScriptValueIfValid( value.property("wait"), scriptable, &command.wait );
        ::fromScriptValueIfValid( value.property("automatic"), scriptable, &command.automatic );
        ::fromScriptValueIfValid( value.property("inMenu"), scriptable, &command.inMenu );
        ::fromScriptValueIfValid( value.property("transform"), scriptable, &command.transform );
        ::fromScriptValueIfValid( value.property("remove"), scriptable, &command.remove );
        ::fromScriptValueIfValid( value.property("hideWindow"), scriptable, &command.hideWindow );
        ::fromScriptValueIfValid( value.property("enable"), scriptable, &command.enable );
        ::fromScriptValueIfValid( value.property("icon"), scriptable, &command.icon );
        ::fromScriptValueIfValid( value.property("shortcuts"), scriptable, &command.shortcuts );
        ::fromScriptValueIfValid( value.property("globalShortcuts"), scriptable, &command.globalShortcuts );
        ::fromScriptValueIfValid( value.property("tab"), scriptable, &command.tab );
        ::fromScriptValueIfValid( value.property("outputTab"), scriptable, &command.outputTab );

        return command;
    }
};

template <>
struct ScriptValueFactory<QVariant> {
    static QScriptValue toScriptValue(const QVariant &variant, Scriptable *scriptable)
    {
        if ( !variant.isValid() )
            return QScriptValue(QScriptValue::UndefinedValue);

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

        return scriptable->engine()->newVariant(variant);
    }

    static QVariant fromScriptValue(const QScriptValue &value, Scriptable *scriptable)
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

template <typename ScriptableClass>
void addScriptableClass(QScriptValue *rootObject, ScriptableClass *cls)
{
    rootObject->setProperty( cls->name(), cls->constructor() );
}

QByteArray readReply(QNetworkReply *reply, const Scriptable &scriptable)
{
    QByteArray data;
    while ( !reply->isFinished() ) {
        if ( !scriptable.isConnected() )
            return QByteArray();
        waitFor(100);
        if ( reply->waitForReadyRead(100) )
            data.append(reply->readAll());
    }
    data.append(reply->readAll());

    return data;
}

QByteArray serializeScriptValue(const QScriptValue &value)
{
    QByteArray data;

    QByteArray *bytes = qscriptvalue_cast<QByteArray*>(value.data());

    if (bytes != nullptr) {
        data = *bytes;
    } else if ( value.isArray() ) {
        const quint32 len = value.property("length").toUInt32();
        for (quint32 i = 0; i < len; ++i)
            data += serializeScriptValue(value.property(i));
    } else if ( !value.isUndefined() ) {
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

Scriptable::Scriptable(
        QScriptEngine *engine,
        ScriptableProxy *proxy,
        QObject *parent)
    : QObject(parent)
    , QScriptable()
    , m_proxy(proxy)
    , m_engine(engine)
    , m_baClass(nullptr)
    , m_dirClass(nullptr)
    , m_fileClass(nullptr)
    , m_temporaryFileClass(nullptr)
    , m_inputSeparator("\n")
    , m_input()
    , m_connected(true)
{
    const QScriptEngine::QObjectWrapOptions opts =
              QScriptEngine::ExcludeChildObjects
            | QScriptEngine::SkipMethodsInEnumeration
            | QScriptEngine::ExcludeSuperClassMethods
            | QScriptEngine::ExcludeSuperClassProperties
            | QScriptEngine::ExcludeSuperClassContents
            | QScriptEngine::ExcludeDeleteLater;

    QScriptValue obj = m_engine->newQObject(this, QScriptEngine::QtOwnership, opts);

    // Keep internal functions as parseInt() or encodeURIComponent().
    QScriptValue oldObj = m_engine->globalObject();
    QScriptValueIterator it(oldObj);
    while (it.hasNext()) {
        it.next();
        obj.setProperty(it.name(), it.value(), it.flags());
    }

    m_engine->setGlobalObject(obj);
    m_engine->setProcessEventsInterval(1000);

    m_baClass = new ByteArrayClass(m_engine);
    addScriptableClass(&obj, m_baClass);

    m_fileClass = new FileClass(m_engine);
    addScriptableClass(&obj, m_fileClass);

    m_temporaryFileClass = new TemporaryFileClass(m_engine);
    addScriptableClass(&obj, m_temporaryFileClass);

    m_dirClass = new DirClass(m_engine);
    addScriptableClass(&obj, m_dirClass);
}

QScriptContext *Scriptable::context() const
{
    return engine()->currentContext();
}

int Scriptable::argumentCount() const
{
    return context()->argumentCount();
}

QScriptValue Scriptable::argument(int index) const
{
    return context()->argument(index);
}

QScriptValue Scriptable::newByteArray(const QByteArray &bytes)
{
    return m_baClass->newInstance(bytes);
}

QByteArray Scriptable::fromString(const QString &value) const
{
  QByteArray bytes = value.toUtf8();
#ifdef COPYQ_OS_WIN
  bytes.replace('\n', "\r\n");
#endif
  return bytes;
}

QVariant Scriptable::toVariant(const QScriptValue &value)
{
    return fromScriptValue<QVariant>(value, this);
}

bool Scriptable::toInt(const QScriptValue &value, int *number) const
{
    bool ok;
    *number = toString(value, this).toInt(&ok);
    return ok;
}

QVariantMap Scriptable::toDataMap(const QScriptValue &value) const
{
    if ( value.isVariant() )
        return value.toVariant().toMap();

    QVariantMap dataMap;

    QScriptValueIterator it(value);
    while (it.hasNext()) {
        it.next();
        if ( it.flags() & QScriptValue::SkipInEnumeration )
            continue;

        dataMap.insert( it.name(), makeByteArray(it.value()) );
    }

    return dataMap;
}

QByteArray Scriptable::makeByteArray(const QScriptValue &value) const
{
    QByteArray *data = getByteArray(value, this);
    return data ? *data : fromString(value.toString());
}

bool Scriptable::toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const
{
    if (mime == mimeItems) {
        const QByteArray *itemData = getByteArray(value, this);
        if (!itemData)
            return false;

        return deserializeData(data, *itemData);
    }

    if (value.isUndefined())
        data->insert( mime, QVariant() );
    else if (!mime.startsWith("text/") && value.scriptClass() == m_baClass)
        data->insert( mime, *getByteArray(value, this) );
    else
        data->insert( mime, toString(value, this).toUtf8() );

    return true;
}

QScriptValue Scriptable::getInputSeparator() const
{
    return m_inputSeparator;
}

void Scriptable::setInputSeparator(const QScriptValue &separator)
{
    m_inputSeparator = toString(separator, this);
}

QString Scriptable::getCurrentPath() const
{
    return m_dirClass ? m_dirClass->getCurrentPath() : QString();
}

void Scriptable::setCurrentPath(const QString &path)
{
    if (m_dirClass)
        m_dirClass->setCurrentPath(path);
    if (m_fileClass)
        m_fileClass->setCurrentPath(path);
    if (m_temporaryFileClass)
        m_temporaryFileClass->setCurrentPath(path);
}

QString Scriptable::getFileName(const QString &fileName) const
{
    return QDir::isRelativePath(fileName) ? getCurrentPath() + '/' + fileName
                                          : fileName;
}

QString Scriptable::arg(int i, const QString &defaultValue)
{
    return i < argumentCount() ? toString(argument(i), this) : defaultValue;
}

void Scriptable::throwError(const QString &errorMessage)
{
    auto currentContext = context();
    if (!currentContext)
        currentContext = engine()->currentContext();
    currentContext->throwError( fromString(errorMessage + '\n') );
}

void Scriptable::sendMessageToClient(const QByteArray &message, int exitCode)
{
    emit sendMessage(message, exitCode);
}

QScriptValue Scriptable::version()
{
    m_skipArguments = 0;
    return tr(programName) + " " COPYQ_VERSION " (hluk@email.cz)\n"
            + tr("Built with: ")
            + "Qt " + QT_VERSION_STR +
            + ", LibQxt " + QXT_VERSION_STR
            + '\n';
}

QScriptValue Scriptable::help()
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

        if ( helpString.isEmpty() ) {
            throwError( tr("Command not found!") );
            return QString();
        }
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

QScriptValue Scriptable::toggle()
{
    m_skipArguments = 0;
    return m_proxy->toggleVisible();
}

void Scriptable::menu()
{
    m_skipArguments = 4;

    if (argumentCount() == 0) {
        m_proxy->toggleMenu();
    } else {
        const auto tabName = toString(argument(0), this);

        int maxItemCount = -1;
        if (argumentCount() >= 2) {
            const auto value = argument(1);
            if ( !toInt(value, &maxItemCount) || maxItemCount <= 0 ) {
                throwError("Argument maxItemCount must be positive number");
                return;
            }
        }

        int x = -1;
        int y = -1;
        if (argumentCount() >= 3) {
            const auto xValue = argument(2);
            const auto yValue = argument(3);
            if ( !toInt(xValue, &x) || x < 0 || !toInt(yValue, &y) || y < 0 ) {
                throwError("Coordinates must be positive numbers");
                return;
            }
        }

        m_proxy->toggleMenu( tabName, maxItemCount, QPoint(x, y) );
    }
}

void Scriptable::exit()
{
    m_skipArguments = 0;
    QByteArray message = fromString( tr("Terminating server.\n") );
    sendMessageToClient(message, CommandPrint);
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

QScriptValue Scriptable::monitoring()
{
    m_skipArguments = 0;
    return m_proxy->isMonitoringEnabled();
}

QScriptValue Scriptable::visible()
{
    m_skipArguments = 0;
    return m_proxy->isMainWindowVisible();
}

QScriptValue Scriptable::focused()
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

QScriptValue Scriptable::clipboard()
{
    m_skipArguments = 1;
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime) );
}

QScriptValue Scriptable::selection()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime, QClipboard::Selection) );
#else
    return QScriptValue();
#endif
}

QScriptValue Scriptable::hasClipboardFormat()
{
    m_skipArguments = 1;
    const QString &mime = arg(0);
    return m_proxy->hasClipboardFormat(mime, QClipboard::Clipboard);
}

QScriptValue Scriptable::hasSelectionFormat()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0);
    return m_proxy->hasClipboardFormat(mime, QClipboard::Selection);
#else
    return false;
#endif
}

QScriptValue Scriptable::copy()
{
    m_skipArguments = -1;
    return copy(QClipboard::Clipboard);
}

QScriptValue Scriptable::copySelection()
{
    m_skipArguments = -1;
#ifdef HAS_MOUSE_SELECTIONS
    return copy(QClipboard::Selection);
#else
    return QScriptValue();
#endif
}

void Scriptable::paste()
{
    m_skipArguments = 0;
    m_proxy->pasteToCurrentWindow();
}

QScriptValue Scriptable::tab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    if ( name.isNull() )
        return toScriptValue( m_proxy->tabs(), this );

    m_proxy->setTab(name);
    return QScriptValue();
}

void Scriptable::removeTab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    const QString error = m_proxy->removeTab(name);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::renameTab()
{
    m_skipArguments = 2;
    const QString &name = arg(0);
    const QString &newName = arg(1);
    const QString error = m_proxy->renameTab(newName, name);
    if ( !error.isEmpty() )
        throwError(error);
}

QScriptValue Scriptable::tabIcon()
{
    m_skipArguments = 2;

    if (argumentCount() == 1)
        return m_proxy->tabIcon(arg(0));

    if (argumentCount() >= 2)
        m_proxy->setTabIcon(arg(0), arg(1));
    else
        throwError(argumentError());

    return QScriptValue();
}

QScriptValue Scriptable::length()
{
    m_skipArguments = 0;
    return m_proxy->browserLength();
}

void Scriptable::select()
{
    m_skipArguments = 1;
    QScriptValue value = argument(0);
    int row;
    if ( toInt(value, &row) )
        m_proxy->browserMoveToClipboard(row);
}

void Scriptable::next()
{
    m_skipArguments = 0;
    nextToClipboard(1);
}

void Scriptable::previous()
{
    m_skipArguments = 0;
    nextToClipboard(-1);
}

void Scriptable::add()
{
    m_skipArguments = -1;

    QStringList texts;

    for (int i = 0; i < argumentCount(); ++i)
        texts.append( toString(argument(i), this) );

    const auto error = m_proxy->browserAdd(texts);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::insert()
{
    m_skipArguments = 2;

    int row;
    if ( !toInt(argument(0), &row) ) {
        throwError(argumentError());
        return;
    }

    QScriptValue value = argument(1);
    QVariantMap data;
    setTextData( &data, toString(value, this) );
    const auto error = m_proxy->browserAdd(data, row);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::remove()
{
    QList<int> rows = getRows();
    m_skipArguments = rows.size();

    if ( rows.empty() )
        rows.append(0);

    const auto error = m_proxy->browserRemoveRows(rows);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::edit()
{
    m_skipArguments = -1;

    QScriptValue value;
    QString text;
    int row = -1;

    const int len = argumentCount();
    for ( int i = 0; i < len; ++i ) {
        value = argument(i);
        if (i > 0)
            text.append(m_inputSeparator);
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

QScriptValue Scriptable::read()
{
    m_skipArguments = -1;

    QByteArray result;
    QString mime(mimeText);
    QScriptValue value;

    bool used = false;
    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, &row) ) {
            if (used)
                result.append( m_inputSeparator.toUtf8() );
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
    QScriptValue value;

    for ( i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if (!toInt(value, &row))
            break;
        if (anyRows)
            text.append(m_inputSeparator);
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

void Scriptable::notification()
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
            if ( !toInt(argument(i), &msec) ) {
                throwError("Expected number after .time argument");
                return;
            }
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
            throwError("Unknown argument: " + name);
            return;
        }
    }

    m_proxy->showMessage(title, message, icon, msec, notificationId, buttons);
}

void Scriptable::exportTab()
{
    m_skipArguments = 1;

    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_proxy->saveTab(getFileName(fileName)) ) {
        throwError( tr("Cannot save to file \"%1\"!").arg(fileName) );
    }
}

void Scriptable::importTab()
{
    m_skipArguments = 1;

    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_proxy->loadTab(getFileName(fileName)) ) {
        throwError(
            tr("Cannot import file \"%1\"!").arg(fileName) );
    }
}

QScriptValue Scriptable::importData()
{
    m_skipArguments = 1;

    const auto fileName = arg(0);
    return m_proxy->importData(fileName);
}

QScriptValue Scriptable::exportData()
{
    m_skipArguments = 1;

    const auto fileName = arg(0);
    return m_proxy->exportData(fileName);
}

QScriptValue Scriptable::config()
{
    m_skipArguments = -1;

    QStringList nameValueInput;

    for (int i = 0; i < argumentCount(); ++i)
        nameValueInput.append( arg(i) );

    const auto nameValue = m_proxy->config(nameValueInput);
    if ( nameValue.size() == 1 )
        return nameValue.first();

    QString errors;
    for (int i = 0; i < nameValue.size(); i += 2) {
        const auto &name = nameValue[i];
        const auto value = nameValue.value(i + 1);
        if ( value.isNull() ) {
            if ( !errors.isEmpty() )
                errors.append('\n');
            errors.append( tr("Invalid option \"%1\"!").arg(name) );
        }
    }

    if ( !errors.isEmpty() ) {
        throwError(errors);
        return QScriptValue();
    }

    if ( nameValue.size() == 2 )
        return nameValue.last();

    QStringList output;
    for (int i = 0; i < nameValue.size(); i += 2) {
        const auto &name = nameValue[i];
        const auto value = nameValue.value(i + 1);
        output.append(name + "=" + value);
    }

    return toScriptValue(output, this);
}

QScriptValue Scriptable::info()
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
#ifdef COPYQ_WS_X11
                "Linux/X11"
#elif defined(Q_OS_WIN)
                "Windows"
#elif defined(Q_OS_MAC)
                "OS X"
#elif defined(Q_OS_LINUX)
                "Linux"
#else
                "?"
#endif
                );

    const QString name = arg(0);
    if (!name.isEmpty())
        return info.value(name);

    QString result;
    for (InfoMap::const_iterator it = info.begin(); it != info.end(); ++it)
        result.append(QString("%1: %2\n").arg(it.key(), it.value()));
    result.chop(1);

    return result;
}

QScriptValue Scriptable::eval()
{
    const auto script = arg(0);
    const auto result = eval(script);
    m_skipArguments = -1;
    return result;    
}

QScriptValue Scriptable::source()
{
    const auto scriptFilePath = arg(0);

    QByteArray script;
    {
        QFile scriptFile( getFileName(scriptFilePath) );
        if ( !scriptFile.open(QIODevice::ReadOnly) ) {
            throwError( QString("Failed to open \"%1\": %2")
                        .arg(scriptFilePath)
                        .arg(scriptFile.errorString()) );
            return QScriptValue();
        }

        script = scriptFile.readAll();
    }

    const auto result = eval(script, scriptFilePath);
    m_skipArguments = 1;
    return result;
}

QScriptValue Scriptable::currentPath()
{
    m_skipArguments = 1;

    if (argumentCount() > 0)
        setCurrentPath(arg(0));

    return getCurrentPath();
}

QScriptValue Scriptable::str(const QScriptValue &value)
{
    m_skipArguments = 1;
    return toString(value, this);
}

QScriptValue Scriptable::input()
{
    m_skipArguments = 0;

    if ( !getByteArray(m_input, this) ) {
        sendMessageToClient(QByteArray(), CommandReadInput);
        while ( m_connected && !getByteArray(m_input, this) )
            QApplication::processEvents();
    }

    return m_input;
}

QScriptValue Scriptable::toUnicode()
{
    m_skipArguments = 2;

    const auto bytes = makeByteArray(argument(0));

    if (argumentCount() >= 2) {
        const auto codec = codecFromNameOrThrow(argument(1));
        if (!codec)
            return QScriptValue();

        return codec->toUnicode(bytes);
    }

    if (argumentCount() >= 1) {
        const auto codec = QTextCodec::codecForUtfText(bytes, nullptr);
        if (!codec) {
            throwError("Failed to detect encoding");
            return QScriptValue();
        }
        return codec->toUnicode(bytes);
    }

    throwError(argumentError());
    return QScriptValue();
}

QScriptValue Scriptable::fromUnicode()
{
    m_skipArguments = 2;

    if (argumentCount() < 2) {
        throwError(argumentError());
        return QScriptValue();
    }

    const auto codec = codecFromNameOrThrow(argument(1));
    if (!codec)
        return QScriptValue();

    const auto text = arg(0);
    return newByteArray( codec->fromUnicode(text) );
}

QScriptValue Scriptable::dataFormats()
{
    m_skipArguments = 0;
    return toScriptValue( m_data.keys(), this );
}

QScriptValue Scriptable::data(const QScriptValue &value)
{
    m_skipArguments = 1;
    return newByteArray( m_data.value(toString(value, this)).toByteArray() );
}

QScriptValue Scriptable::setData()
{
    m_skipArguments = 2;

    const QString mime = arg(0);
    if ( !toItemData(argument(1), mime, &m_data) )
        return false;

    if ( m_data.value(mimeSelectedItems).isValid() )
        m_proxy->setSelectedItemsData(mime, m_data.value(mime));

    return true;
}

QScriptValue Scriptable::removeData()
{
    m_skipArguments = 1;

    const QString mime = arg(0);
    m_data.remove(mime);
    m_proxy->setSelectedItemsData(mime, QVariant());
    return true;
}

void Scriptable::print(const QScriptValue &value)
{
    m_skipArguments = 1;
    sendMessageToClient(makeByteArray(value), CommandPrint);
}

void Scriptable::abort()
{
    m_skipArguments = 0;

    QScriptEngine *eng = engine() ? engine() : m_engine;
    if (eng)
        eng->abortEvaluation();
}

void Scriptable::fail()
{
    m_skipArguments = 0;
    sendMessageToClient("", CommandError);
}

#ifdef HAS_TESTS
void Scriptable::keys()
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
            if (!m_connected) {
                throwError("Disconnected");
                return;
            }
        }
    }
}

QScriptValue Scriptable::testSelected()
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

QScriptValue Scriptable::testSelected()
{
    m_skipArguments = 0;
    return QScriptValue();
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

QScriptValue Scriptable::selectItems()
{
    QList<int> rows = getRows();
    m_skipArguments = rows.size();
    return m_proxy->selectItems(rows);
}

QScriptValue Scriptable::selectedTab()
{
    m_skipArguments = 0;
    return m_data.value(mimeCurrentTab).toString();
}

QScriptValue Scriptable::selectedItems()
{
    m_skipArguments = 0;
    return toScriptValue( m_proxy->selectedItems(), this );
}

QScriptValue Scriptable::currentItem()
{
    m_skipArguments = 0;
    return m_proxy->currentItem();
}

QScriptValue Scriptable::selectedItemData()
{
    int selectedIndex;
    if ( !toInt(argument(0), &selectedIndex) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    return toScriptValue( m_proxy->selectedItemData(selectedIndex), this );
}

QScriptValue Scriptable::setSelectedItemData()
{
    int selectedIndex;
    if ( !toInt(argument(0), &selectedIndex) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    const auto data = toDataMap( argument(1) );
    return toScriptValue( m_proxy->setSelectedItemData(selectedIndex, data), this );
}

QScriptValue Scriptable::selectedItemsData()
{
    return toScriptValue( m_proxy->selectedItemsData(), this );
}

void Scriptable::setSelectedItemsData()
{
    m_skipArguments = 1;
    const auto dataList = fromScriptValue<QList<QVariantMap>>( argument(0), this );
    m_proxy->setSelectedItemsData(dataList);
}

QScriptValue Scriptable::escapeHtml()
{
    m_skipArguments = 1;
    return ::escapeHtml( toString(argument(0), this) );
}

QScriptValue Scriptable::unpack()
{
    m_skipArguments = 1;

    QVariantMap data;

    if ( !toItemData(argument(0), mimeItems, &data) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    return toScriptValue(data, this);
}

QScriptValue Scriptable::pack()
{
    m_skipArguments = 1;
    QVariantMap data = toDataMap( argument(0) );
    return newByteArray(serializeData(data));
}

QScriptValue Scriptable::getItem()
{
    m_skipArguments = 1;

    int row;
    if ( !toInt(argument(0), &row) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    return toScriptValue( m_proxy->browserItemData(row), this );
}

void Scriptable::setItem()
{
    m_skipArguments = 2;

    int row;
    if ( !toInt(argument(0), &row) ) {
        throwError(argumentError());
        return;
    }

    QVariantMap data = toDataMap( argument(1) );
    const auto error = m_proxy->browserAdd(data, row);
    if ( !error.isEmpty() )
        throwError(error);
}

QScriptValue Scriptable::toBase64()
{
    m_skipArguments = 1;
    return QString::fromLatin1(makeByteArray(argument(0)).toBase64());
}

QScriptValue Scriptable::fromBase64()
{
    m_skipArguments = 1;
    return newByteArray(QByteArray::fromBase64(makeByteArray(argument(0))));
}

QScriptValue Scriptable::open()
{
    m_skipArguments = -1;

    for ( int i = 0; i < argumentCount(); ++i ) {
        if ( !QDesktopServices::openUrl(QUrl(toString(argument(i), this))) )
            return false;
    }

    return true;
}

QScriptValue Scriptable::execute()
{
    m_skipArguments = -1;

    m_executeStdoutCallback = QScriptValue();

    // Pass all arguments until null to command. The rest will be sent to stdin.
    QStringList args;
    int i = 0;
    for ( ; i < argumentCount(); ++i ) {
        const auto arg = argument(i);
        if (arg.isNull())
            break;

        if ( arg.isFunction() )
            m_executeStdoutCallback = arg;
        else
            args.append( toString(arg, this) );
    }

    Action action;
    for ( ++i ; i < argumentCount(); ++i ) {
        const auto arg = argument(i);
        if ( arg.isFunction() )
            m_executeStdoutCallback = arg;
        else
            action.setInput( action.input() + makeByteArray(arg) );
    }

    if ( m_executeStdoutCallback.isFunction() ) {
        action.setItemSeparator(QRegExp("\n"));
        action.setOutputFormat(mimeText);
        connect( &action, SIGNAL(newItems(QStringList, QString, QString)),
                 this, SLOT(onExecuteOutput(QStringList)) );
    } else {
        action.setOutputFormat("DATA");
    }

    action.setCommand(args);
    action.setWorkingDirectory( m_dirClass->getCurrentPath() );
    action.start();

    if ( !action.waitForStarted(5000) )
        return QScriptValue();

    while ( !action.waitForFinished(5000) && m_connected ) {}

    if ( action.isRunning() && !action.waitForFinished(5000) ) {
        action.terminate();
        return QScriptValue();
    }

    if ( action.actionFailed() )
        return QScriptValue();

    QScriptValue actionResult = m_engine->newObject();
    actionResult.setProperty( "stdout", newByteArray(action.outputData()) );
    actionResult.setProperty( "stderr", action.errorOutput() );
    actionResult.setProperty( "exit_code", action.exitCode() );

    return actionResult;
}

QScriptValue Scriptable::currentWindowTitle()
{
    m_skipArguments = 0;
    return m_proxy->currentWindowTitle();
}

QScriptValue Scriptable::dialog()
{
    m_skipArguments = -1;

    NamedValueList values;
    values.reserve(argumentCount() / 2);

    for ( int i = 0; i < argumentCount(); i += 2 ) {
        const QString key = arg(i);
        const QScriptValue value = argument(i + 1);
        values.append( NamedValue(key, toVariant(value)) );
    }

    values = m_proxy->inputDialog(values);

    if (values.isEmpty())
        return QScriptValue();

    if (values.size() == 1)
        return toScriptValue( values.first().value, this );

    QScriptValue result = m_engine->newObject();

    for (const auto &value : values)
        result.setProperty( value.name, toScriptValue(value.value, this) );

    return result;
}

QScriptValue Scriptable::settings()
{
    m_skipArguments = 2;

    if (argumentCount() == 2) {
        const QString key = arg(0);
        const QScriptValue value = argument(1);
        const QVariant saveValue = toVariant(value);
        m_proxy->setUserValue(key, saveValue);

        return QScriptValue();
    }

    QSettings settings;
    settings.beginGroup("script");

    if (argumentCount() == 1) {
        const auto value = settings.value(arg(0));
        return toScriptValue(value, this);
    }

    return toScriptValue(settings.allKeys(), this);
}

QScriptValue Scriptable::dateString()
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

QScriptValue Scriptable::commands()
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

QScriptValue Scriptable::importCommands()
{
    m_skipArguments = 1;
    const auto commands = importCommandsFromText(arg(0));
    return toScriptValue(commands, this);
}

QScriptValue Scriptable::exportCommands()
{
    m_skipArguments = 1;
    const auto commands = fromScriptValue<QList<Command>>(argument(0), this);

    const auto exportedCommands = ::exportCommands(commands);
    if ( exportedCommands.isEmpty() ) {
        throwError("Failed to export commands");
        return QScriptValue();
    }

    return exportedCommands;
}

QScriptValue Scriptable::networkGet()
{
    m_skipArguments = 1;
    const QString url = arg(0);
    return NetworkReply::get(url, this);
}

QScriptValue Scriptable::networkPost()
{
    m_skipArguments = 2;
    const QString url = arg(0);
    const QByteArray postData = makeByteArray(argument(1));
    return NetworkReply::post(url, postData, this);
}

QScriptValue Scriptable::env()
{
    m_skipArguments = 1;
    const QString name = arg(0);
    const QByteArray value = qgetenv(name.toUtf8().constData());
    return newByteArray(value);
}

QScriptValue Scriptable::setEnv()
{
    m_skipArguments = 2;
    const QString name = arg(0);
    const QByteArray value = makeByteArray(argument(1));
    return qputenv(name.toUtf8().constData(), value);
}

void Scriptable::sleep()
{
    m_skipArguments = 1;

    class ThreadSleep : public QThread {
    public:
        static void msleep(unsigned long msec) {
            QThread::msleep(msec);
        }
    };

    int msec;
    if ( !toInt(argument(0), &msec) ) {
        throwError(argumentError());
        return;
    }

    if (msec > 0)
        ThreadSleep::msleep( static_cast<unsigned long>(msec) );
}

QVariant Scriptable::call(const QString &method, const QVariantList &arguments)
{
    if ( m_engine->hasUncaughtException() )
        return QVariant();

    m_skipArguments = 2;

    QScriptValueList fnArgs;
    for (const auto &argument : arguments)
        fnArgs.append( toScriptValue(argument, this) );

    auto fn = m_engine->globalObject().property(method);
    const auto result = fn.call(QScriptValue(), fnArgs);
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

QScriptValue Scriptable::screenshot()
{
    return screenshot(false);
}

QScriptValue Scriptable::screenshotSelect()
{
    return screenshot(true);
}

void Scriptable::onMessageReceived(const QByteArray &bytes, int messageCode)
{
    COPYQ_LOG( "Message received: " + messageCodeToString(messageCode) );

    if (messageCode == CommandArguments)
        executeArguments(bytes);
    else if (messageCode == CommandReadInputReply)
        m_input = newByteArray(bytes);
    else
        log("Incorrect message code from client", LogError);
}

void Scriptable::onDisconnected()
{
    m_connected = false;
}

void Scriptable::onExecuteOutput(const QStringList &lines)
{
    if ( m_executeStdoutCallback.isFunction() ) {
        const auto arg = toScriptValue(lines, this);
        m_executeStdoutCallback.call( QScriptValue(), QScriptValueList() << arg );
    }
}

void Scriptable::executeArguments(const QByteArray &bytes)
{
    Arguments args;
    QDataStream stream(bytes);
    stream >> args;
    if ( stream.status() != QDataStream::Ok ) {
        log("Failed to read client arguments", LogError);
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

    const QString currentPath = getTextData(args.at(Arguments::CurrentPath));
    m_fileClass->setCurrentPath(currentPath);
    m_temporaryFileClass->setCurrentPath(currentPath);
    m_dirClass->setCurrentPath(currentPath);

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
        QScriptValueList fnArgs;
        bool readRaw = false;
        for ( int i = Arguments::Rest; i < args.length(); ++i ) {
            const QByteArray &arg = args.at(i);
            if (!readRaw && arg == "--") {
                readRaw = true;
            } else {
                const QScriptValue value = readRaw || arg != "-"
                        ? newByteArray(arg)
                        : input();
                fnArgs.append(value);
            }
        }

        QString cmd;
        QScriptValue result;

        int skipArguments = 0;
        while ( skipArguments < fnArgs.size() && !m_engine->hasUncaughtException() ) {
            if ( result.isFunction() ) {
                m_skipArguments = -1;
                result = result.call( QScriptValue(), fnArgs.mid(skipArguments) );
                if (m_skipArguments == -1)
                    break;
                skipArguments += m_skipArguments;
            } else {
                cmd = toString(fnArgs[skipArguments], this);
                result = eval(cmd);
                ++skipArguments;
            }
        }

        if ( result.isFunction() && !m_engine->hasUncaughtException() )
            result = result.call( QScriptValue(), fnArgs.mid(skipArguments) );

        if ( m_engine->hasUncaughtException() ) {
            const auto exceptionText = processUncaughtException(cmd);
            response = createScriptErrorMessage(exceptionText).toUtf8();
            exitCode = CommandException;
        } else {
            response = serializeScriptValue(result);
            exitCode = CommandFinished;
        }
    }

    if (exitCode == CommandFinished && hasData && oldData != m_data)
        m_proxy->setActionData(id, data());

    // Destroy objects so destructors are run before script finishes
    // (e.g. file writes are flushed or temporary files are automatically removed).
    m_engine->collectGarbage();

    sendMessageToClient(response, exitCode);

    COPYQ_LOG("DONE");
}

QString Scriptable::processUncaughtException(const QString &cmd)
{
    if ( !m_engine->hasUncaughtException() )
        return QString();

    const auto exceptionName = m_engine->uncaughtException().toString()
            .remove(QRegExp("^Error: "))
            .trimmed();

    auto backtrace = m_engine->uncaughtExceptionBacktrace();
    for (int i = backtrace.size() - 1; i >= 0; --i) {
        if ( backtrace[i].endsWith("-1") )
            backtrace.removeAt(i);
    }

    QString backtraceText;
    if ( !backtrace.isEmpty() )
        backtraceText = "\n\n--- backtrace ---\n" + backtrace.join("\n") + "\n--- end backtrace ---";

    const auto exceptionText = exceptionName + backtraceText;

    logScriptError(
                QString("Exception in command \"%1\": %2")
                .arg(cmd, exceptionText) );

    showExceptionMessage(exceptionName);

    return exceptionText;
}

void Scriptable::showExceptionMessage(const QString &message)
{
    if (!m_proxy)
        return;

    const auto title = m_actionName.isEmpty()
        ? tr("Exception")
        : tr("Exception in %1").arg( quoteString(m_actionName) );
    m_proxy->showMessage(title, message, QString(QChar(IconWarningSign)), 8000);
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

QScriptValue Scriptable::copy(QClipboard::Mode mode)
{
    const int args = argumentCount();
    QVariantMap data;

    if (args == 0) {
        // Reset clipboard first.
        const QString mime = COPYQ_MIME_PREFIX "invalid";
        const QByteArray value = "invalid";
        data.insert(mime, value);
        if ( !setClipboard(&data, mode) )
            return false;

        m_proxy->copyFromCurrentWindow();

        // Wait for clipboard to be set.
        for (int i = 0; i < 10; ++i) {
            if ( m_proxy->getClipboardData(mime) != value )
                return true;
            waitFor(5 + i * 25);
        }

        throwError( tr("Failed to copy!") );
        return false;
    }

    if (args == 1) {
        QScriptValue value = argument(0);
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

    throwError(argumentError());
    return false;
}

bool Scriptable::setClipboard(QVariantMap *data, QClipboard::Mode mode)
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

    throwError( tr("Failed to set clipboard!") );
    return false;
}

void Scriptable::changeItem(bool create)
{
    int row;
    int args = argumentCount();
    int i;

    // [ROW]
    if ( toInt(argument(0), &row) ) {
        if (args < 3 || args % 2 != 1 ) {
            throwError(argumentError());
            return;
        }
        i = 1;
    } else {
        if (args < 2 || args % 2 != 0 ) {
            throwError(argumentError());
            return;
        }
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
            throwError(error);
    } else {
        m_proxy->browserChange(data, row);
    }
}

void Scriptable::nextToClipboard(int where)
{
    QVariantMap data = m_proxy->nextItem(where);
    if (data.isEmpty())
        return;

    setClipboard(&data, QClipboard::Clipboard);
#ifdef HAS_MOUSE_SELECTIONS
    setClipboard(&data, QClipboard::Selection);
#endif
}

QScriptValue Scriptable::screenshot(bool select)
{
    m_skipArguments = 2;

    const auto format = arg(0, "png");
    const auto screen = arg(1);
    const auto imageData = m_proxy->screenshot(format, screen, select);

    if ( imageData.isEmpty() ) {
        throwError("Failed to grab screenshot");
        return QScriptValue();
    }

    return newByteArray(imageData);
}

QScriptValue Scriptable::eval(const QString &script, const QString &fileName)
{
    const auto syntaxResult = engine()->checkSyntax(script);
    if (syntaxResult.state() != QScriptSyntaxCheckResult::Valid) {
        throwError( QString("%1:%2:%3: syntax error: %4")
                    .arg(fileName)
                    .arg(syntaxResult.errorLineNumber())
                    .arg(syntaxResult.errorColumnNumber())
                    .arg(syntaxResult.errorMessage()) );
        return QScriptValue();
    }

    return engine()->evaluate(script, fileName);
}

QScriptValue Scriptable::eval(const QString &script)
{
    const int i = script.indexOf('\n');
    const QString name = quoteString( i == -1 ? script : script.mid(0, i) + "..." );
    return eval(script, name);
}

QTextCodec *Scriptable::codecFromNameOrThrow(const QScriptValue &codecName)
{
    const auto codec = QTextCodec::codecForName( makeByteArray(codecName) );
    if (!codec) {
        QString codecs;
        for (const auto &availableCodecName : QTextCodec::availableCodecs())
            codecs.append( "\n" + QString::fromUtf8(availableCodecName) );
        throwError("Available codecs are:" + codecs);
    }
    return codec;
}

QScriptValue NetworkReply::get(const QString &url, Scriptable *scriptable)
{
    NetworkReply *reply = new NetworkReply(url, QByteArray(), scriptable);
    return reply->toScriptValue();
}

QScriptValue NetworkReply::post(const QString &url, const QByteArray &postData, Scriptable *scriptable)
{
    auto reply = new NetworkReply(url, postData, scriptable);
    return reply->toScriptValue();
}

NetworkReply::~NetworkReply()
{
    if (m_reply)
        m_reply->deleteLater();
    if (m_replyHead)
        m_replyHead->deleteLater();
}

QScriptValue NetworkReply::data()
{
    if (m_data.isValid())
        return m_data;

    const QByteArray data = readReply(m_reply, *m_scriptable);
    m_data = m_scriptable->newByteArray(data);

    return m_data;
}

QScriptValue NetworkReply::error() const
{
    if (m_reply->error() != QNetworkReply::NoError)
        return m_reply->errorString();

    if (m_replyHead && m_replyHead->error() != QNetworkReply::NoError)
        return m_replyHead->errorString();

    return QScriptValue();
}

QScriptValue NetworkReply::status() const
{
    const QVariant v = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (v.isValid())
        return v.toInt();
    return QScriptValue();
}

QScriptValue NetworkReply::redirect() const
{
    const QVariant v = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (v.isValid())
        return v.toUrl().resolved(m_reply->url()).toString();
    return QScriptValue();
}

QScriptValue NetworkReply::headers()
{
    fetchHeaders();

    QScriptValue headers = m_scriptable->engine()->newArray();
    int i = 0;
    for ( const auto &header : m_replyHead->rawHeaderList() ) {
        QScriptValue pair = m_scriptable->engine()->newArray();
        pair.setProperty( 0, m_scriptable->newByteArray(header) );
        pair.setProperty( 1, m_scriptable->newByteArray(m_replyHead->rawHeader(header)) );
        headers.setProperty( static_cast<quint32>(i), pair );
        ++i;
    }

    return headers;
}

NetworkReply::NetworkReply(const QString &url, const QByteArray &postData, Scriptable *scriptable)
    : QObject(scriptable)
    , m_scriptable(scriptable)
    , m_manager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_replyHead(nullptr)
{
    if (postData.isEmpty())
        m_reply = m_manager->get(QNetworkRequest(url));
    else
        m_reply = m_manager->post(QNetworkRequest(url), postData);
}

QScriptValue NetworkReply::toScriptValue()
{
    return m_scriptable->engine()->newQObject(this, QScriptEngine::ScriptOwnership);
}

void NetworkReply::fetchHeaders()
{
    if (m_replyHead)
        return;

    // Omit reading all data just to retrieve headers.
    if (m_data.isValid()) {
        m_replyHead = m_reply;
    } else {
        m_replyHead = m_manager->head(m_reply->request());
        readReply(m_replyHead, *m_scriptable);
    }
}

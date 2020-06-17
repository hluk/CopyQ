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

#include "scriptable.h"

#include "app/clipboardmonitor.h"
#include "common/action.h"
#include "common/command.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/log.h"
#include "common/sleeptimer.h"
#include "common/version.h"
#include "common/textdata.h"
#include "gui/icons.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"
#include "scriptable/commandhelp.h"
#include "scriptable/dirclass.h"
#include "scriptable/fileclass.h"
#include "scriptable/scriptableproxy.h"
#include "scriptable/temporaryfileclass.h"
#include "../qt/bytearrayclass.h"

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPoint>
#include <QRegularExpression>
#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>
#include <QVector>
#include <QTextCodec>
#include <QThread>
#include <QTimer>

Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

namespace {

const char *const programName = "CopyQ Clipboard Manager";
const char *const mimeIgnore = COPYQ_MIME_PREFIX "ignore";

class PerformanceLogger {
public:
    explicit PerformanceLogger(const QString &label)
        : m_label(label)
    {
        m_timer.start();
    }

    ~PerformanceLogger()
    {
        const qint64 ms = m_timer.elapsed();
        if (ms >= 5000)
            log(ms, LogWarning);
        else if (ms >= 500)
            log(ms, LogNote);
        else if (ms >= 150)
            log(ms, LogDebug);
        else
            log(ms, LogTrace);
    }

private:
    void log(qint64 ms, LogLevel level) const
    {
        if ( !hasLogLevel(level) )
            return;

        ::log( QString("%1: %2")
               .arg(m_label, "Finished in %1 ms")
               .arg(ms), level );
    }

    QString m_label;
    QElapsedTimer m_timer;
};

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
    static QScriptValue toScriptValue(const T &value, const Scriptable *)
    {
        return QScriptValue(value);
    }

    static T fromScriptValue(const QScriptValue &value, const Scriptable *)
    {
        const auto variant = toVariant(value);
        Q_ASSERT( variant.canConvert<T>() );
        return variant.value<T>();
    }
};

template <typename T>
QScriptValue toScriptValue(const T &value, const Scriptable *scriptable)
{
    return ScriptValueFactory<T>::toScriptValue(value, scriptable);
}

template <typename T>
T fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
{
    return ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename T>
void fromScriptValueIfValid(const QScriptValue &value, const Scriptable *scriptable, T *outputValue)
{
    if (value.isValid())
        *outputValue = ScriptValueFactory<T>::fromScriptValue(value, scriptable);
}

template <typename List, typename T>
struct ScriptValueListFactory {
    static QScriptValue toScriptValue(const List &list, const Scriptable *scriptable)
    {
        QScriptValue array = scriptable->engine()->newArray();
        for ( int i = 0; i < list.size(); ++i ) {
            const auto value = ScriptValueFactory<T>::toScriptValue(list[i], scriptable);
            array.setProperty( static_cast<quint32>(i), value );
        }
        return array;
    }

    static List fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
    {
        if ( !value.isArray() )
            return List();

        const quint32 length = value.property("length").toUInt32();
        List list;
        for ( quint32 i = 0; i < length; ++i ) {
            const auto item = value.property(i);
            list.append( ScriptValueFactory<T>::fromScriptValue(item, scriptable) );
        }
        return list;
    }
};

template <typename T>
struct ScriptValueFactory< QList<T> > : ScriptValueListFactory< QList<T>, T > {};

template <typename T>
struct ScriptValueFactory< QVector<T> > : ScriptValueListFactory< QVector<T>, T > {};

template <>
struct ScriptValueFactory<QVariantMap> {
    static QScriptValue toScriptValue(const QVariantMap &dataMap, const Scriptable *scriptable)
    {
        QScriptValue value = scriptable->engine()->newObject();

        for (auto it = dataMap.constBegin(); it != dataMap.constEnd(); ++it)
            value.setProperty( it.key(), ::toScriptValue(it.value(), scriptable) );

        return value;
    }

    static QVariantMap fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
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
    static QScriptValue toScriptValue(const QByteArray &bytes, const Scriptable *scriptable)
    {
        return scriptable->byteArrayClass()->newInstance(bytes);
    }
};

template <>
struct ScriptValueFactory<QStringList> {
    static QScriptValue toScriptValue(const QStringList &list, const Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::toScriptValue(list, scriptable);
    }

    static QStringList fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::fromScriptValue(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QString> {
    static QScriptValue toScriptValue(const QString &text, const Scriptable *)
    {
        return QScriptValue(text);
    }

    static QString fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
    {
        return toString(value, scriptable);
    }
};

template <>
struct ScriptValueFactory<QRegularExpression> {
    static QScriptValue toScriptValue(const QRegularExpression &re, const Scriptable *scriptable)
    {
        return scriptable->engine()->newVariant(re);
    }

    static QRegularExpression fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
    {
        if (value.isVariant()) {
            const auto variant = value.toVariant();
            return variant.toRegularExpression();
        }

        return QRegularExpression( toString(value, scriptable) );
    }
};

template <>
struct ScriptValueFactory<Command> {
    static QScriptValue toScriptValue(const Command &command, const Scriptable *scriptable)
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
        value.setProperty("display", command.display);
        value.setProperty("inMenu", command.inMenu);
        value.setProperty("isGlobalShortcut", command.isGlobalShortcut);
        value.setProperty("isScript", command.isScript);
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

    static Command fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
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
        ::fromScriptValueIfValid( value.property("display"), scriptable, &command.display );
        ::fromScriptValueIfValid( value.property("inMenu"), scriptable, &command.inMenu );
        ::fromScriptValueIfValid( value.property("isGlobalShortcut"), scriptable, &command.isGlobalShortcut );
        ::fromScriptValueIfValid( value.property("isScript"), scriptable, &command.isScript );
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
    static QScriptValue toScriptValue(const QVariant &variant, const Scriptable *scriptable)
    {
        if ( !variant.isValid() )
            return QScriptValue(QScriptValue::UndefinedValue);

        if (variant.type() == QVariant::Bool)
            return ::toScriptValue(variant.toBool(), scriptable);

        if (variant.type() == QVariant::ByteArray)
            return ::toScriptValue(variant.toByteArray(), scriptable);

        if (variant.type() == QVariant::String)
            return ::toScriptValue(variant.toString(), scriptable);

        if (variant.type() == QVariant::Char)
            return ::toScriptValue(variant.toString(), scriptable);

        Q_ASSERT(variant.type() != QVariant::RegExp);
        if (variant.type() == QVariant::RegularExpression)
            return ::toScriptValue(variant.toRegularExpression(), scriptable);

        if (variant.canConvert<QVariantList>())
            return ::toScriptValue(variant.value<QVariantList>(), scriptable);

        if (variant.canConvert<QVariantMap>())
            return ::toScriptValue(variant.value<QVariantMap>(), scriptable);

        return scriptable->engine()->newVariant(variant);
    }

    static QVariant fromScriptValue(const QScriptValue &value, const Scriptable *scriptable)
    {
        auto bytes = getByteArray(value, scriptable);
        if (bytes)
            return QVariant(*bytes);

        auto file = getFile(value, scriptable);
        if (file) {
            const QFileInfo fileInfo(*file);
            const auto path = fileInfo.absoluteFilePath();
            return QVariant::fromValue( ScriptablePath{path} );
        }

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
        if ( !scriptable.canContinue() )
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

QString parseCommandLineArgument(const QString &arg)
{
    QString result;
    bool escape = false;

    for (const auto &c : arg) {
        if (escape) {
            escape = false;

            if (c == 'n')
                result.append('\n');
            else if (c == 't')
                result.append('\t');
            else if (c == '\\')
                result.append('\\');
            else
                result.append(c);
        } else if (c == '\\') {
            escape = true;
        } else {
            result.append(c);
        }
    }

    return result;
}

bool matchData(const QRegularExpression &re, const QVariantMap &data, const QString &format)
{
    if ( re.pattern().isEmpty() )
        return true;

    const QString text = getTextData(data, format);
    return text.contains(re);
}

bool isInternalDataFormat(const QString &format)
{
    return format == mimeWindowTitle
        || format == mimeItems
        || format == mimeOwner
        || format == mimeClipboardMode
        || format == mimeCurrentTab
        || format == mimeSelectedItems
        || format == mimeCurrentItem
        || format == mimeShortcut
        || format == mimeOutputTab;
}

QVariantMap copyWithoutInternalData(const QVariantMap &data) {
    QVariantMap newData;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const auto &format = it.key();
        if ( !isInternalDataFormat(format) )
            newData.insert(format, it.value());
    }

    return newData;
}

QScriptValue checksumForArgument(Scriptable *scriptable, QCryptographicHash::Algorithm method)
{
    const auto data = scriptable->makeByteArray(scriptable->argument(0));
    const QByteArray hash = QCryptographicHash::hash(data, method).toHex();
    return QString::fromUtf8(hash);
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
    const auto oldObj = m_engine->globalObject();
    obj.setPrototype(oldObj);

    m_engine->setGlobalObject(obj);
    m_engine->setProcessEventsInterval(1000);

    const auto objectPrototype = oldObj.property("Object").property("prototype");

    m_baClass = new ByteArrayClass(m_engine, objectPrototype);
    addScriptableClass(&obj, m_baClass);

    m_fileClass = new FileClass(m_engine, objectPrototype);
    addScriptableClass(&obj, m_fileClass);

    m_temporaryFileClass = new TemporaryFileClass(m_engine, objectPrototype);
    addScriptableClass(&obj, m_temporaryFileClass);

    m_dirClass = new DirClass(m_engine, objectPrototype);
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

QScriptValue Scriptable::fromDataMap(const QVariantMap &dataMap) const
{
    return toScriptValue(dataMap, this);
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

QString Scriptable::getAbsoluteFilePath(const QString &fileName) const
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

void Scriptable::throwSaveError(const QString &filePath)
{
    throwError( tr("Cannot save to file \"%1\"!").arg(filePath) );
}

void Scriptable::throwImportError(const QString &filePath)
{
    throwError( tr("Cannot import file \"%1\"!").arg(filePath) );
}

QScriptValue Scriptable::getGlobal()
{
    return m_engine->globalObject();
}

QScriptValue Scriptable::getPlugins()
{
    // Load plugins on demand.
    if ( !m_plugins.isValid() ) {
        ItemFactory factory;
        factory.loadPlugins();

        QSettings settings;
        factory.loadItemFactorySettings(&settings);

        const auto scriptableObjects = factory.scriptableObjects();

        m_plugins = m_engine->newObject();

        for (auto obj : scriptableObjects) {
            const auto value = m_engine->newQObject(obj, QScriptEngine::ScriptOwnership);
            const auto name = obj->objectName();
            m_plugins.setProperty(name, value);
            obj->setScriptable(this);
            obj->start();
        }
    }

    return m_plugins;
}

QScriptValue Scriptable::call(QScriptValue *fn, const QScriptValue &object, const QVariantList &arguments) const
{
    QScriptValueList fnArgs;
    fnArgs.reserve( arguments.size() );
    for (const auto &argument : arguments)
        fnArgs.append( toScriptValue(argument, this) );

    return fn->call(object, fnArgs);
}

QScriptValue Scriptable::version()
{
    m_skipArguments = 0;
    return tr(programName) + " " COPYQ_VERSION "\n"
            + "Qt: " QT_VERSION_STR "\n"
            + "Compiler: "
#if defined(Q_CC_GNU)
            "GCC"
#elif defined(Q_CC_CLANG)
            "Clang"
#elif defined(Q_CC_MINGW)
            "MinGW"
#elif defined(Q_CC_MSVC)
            "MSVC"
#else
            "???"
#endif
            + "\n"
#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
            + "Arch: " + QSysInfo::buildAbi() + "\n"
            + "OS: " + QSysInfo::prettyProductName() + "\n"
#endif
            ;
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
            + " " + COPYQ_VERSION + "\n");
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
        m_proxy->toggleCurrentMenu();
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
            if ( !toInt(xValue, &x) || !toInt(yValue, &y) ) {
                throwError("Coordinates must be numbers");
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
    print(message);
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

QScriptValue Scriptable::preview()
{
    m_skipArguments = 1;
    return m_proxy->preview( toVariant(argument(0)) );
}

QScriptValue Scriptable::filter()
{
    m_skipArguments = 1;
    if (argumentCount() == 0)
        return m_proxy->filter();

    m_proxy->filter(arg(0));
    return QScriptValue();
}

void Scriptable::ignore()
{
    m_skipArguments = 0;
    m_data[mimeIgnore] = QByteArray();
}

QScriptValue Scriptable::clipboard()
{
    m_skipArguments = 1;
    const QString &mime = arg(0, mimeText);
    return newByteArray( getClipboardData(mime) );
}

QScriptValue Scriptable::selection()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0, mimeText);
    return newByteArray( getClipboardData(mime, ClipboardMode::Selection) );
#else
    return QScriptValue();
#endif
}

QScriptValue Scriptable::hasClipboardFormat()
{
    m_skipArguments = 1;
    const QString &mime = arg(0);
    return hasClipboardFormat(mime, ClipboardMode::Clipboard);
}

QScriptValue Scriptable::hasSelectionFormat()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0);
    return hasClipboardFormat(mime, ClipboardMode::Selection);
#else
    return false;
#endif
}

QScriptValue Scriptable::isClipboard()
{
    return isClipboardData(m_data);
}

QScriptValue Scriptable::copy()
{
    m_skipArguments = -1;
    return copy(ClipboardMode::Clipboard);
}

QScriptValue Scriptable::copySelection()
{
    m_skipArguments = -1;
#ifdef HAS_MOUSE_SELECTIONS
    return copy(ClipboardMode::Selection);
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

    m_tabName = name;
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

QScriptValue Scriptable::unload()
{
    const auto tabs = arguments();
    const QStringList unloaded = m_proxy->unloadTabs(tabs.isEmpty() ? m_proxy->tabs() : tabs);
    return toScriptValue(unloaded, this);
}

void Scriptable::forceUnload()
{
    const auto tabs = arguments();
    m_proxy->forceUnloadTabs(tabs.isEmpty() ? m_proxy->tabs() : tabs);
}

QScriptValue Scriptable::length()
{
    m_skipArguments = 0;
    return m_proxy->browserLength(m_tabName);
}

void Scriptable::select()
{
    m_skipArguments = 1;
    QScriptValue value = argument(0);
    int row;
    if ( !toInt(value, &row) )
        throwError(argumentError());

    m_proxy->browserMoveToClipboard(m_tabName, row);
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
    insert(0, 0, argumentCount());
}

void Scriptable::insert()
{
    insert(argumentCount());
}

void Scriptable::remove()
{
    auto rows = getRows();
    m_skipArguments = rows.size();

    if ( rows.empty() )
        rows.append(0);

    const auto error = m_proxy->browserRemoveRows(m_tabName, rows);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::move()
{
    m_skipArguments = 1;

    int row;
    if ( !toInt(argument(0), &row) ) {
        throwError(argumentError());
        return;
    }

    m_proxy->browserMoveSelected(row);
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
            const QByteArray bytes = row >= 0 ? m_proxy->browserItemData(m_tabName, row, mimeText)
                                              : getClipboardData(mimeText);
            text.append( getTextData(bytes) );
        } else {
            text.append( toString(value, this) );
        }
    }

    bool changeClipboard = row < 0;

    if ( !m_proxy->browserOpenEditor(m_tabName, fromString(text), changeClipboard) ) {
        m_proxy->showBrowser(m_tabName);
        if (len == 1 && row >= 0) {
            m_proxy->browserSetCurrent(m_tabName, row);
            m_proxy->browserEditRow(m_tabName, row);
        } else {
            m_proxy->browserEditNew(m_tabName, text, changeClipboard);
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
            result.append( row >= 0 ? m_proxy->browserItemData(m_tabName, row, mime)
                                    : getClipboardData(mime) );
        } else {
            mime = toString(value, this);
        }
    }

    if (!used)
        result.append( getClipboardData(mime) );

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
        text.append( getTextData(m_proxy->browserItemData(m_tabName, row, mimeText)) );
    }

    m_skipArguments = i + 2;

    if (!anyRows) {
        text = getTextData( getClipboardData(mimeText) );
    }

    const QVariantMap data = createDataMap(mimeText, text);

    if (i < argumentCount()) {
        Command command;
        command.cmd = toString(value, this);
        command.output = mimeText;
        command.input = mimeText;
        command.wait = false;
        command.outputTab = m_proxy->tab(m_tabName);
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
    const auto icon = QString(QChar(IconInfoCircle));
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
    auto icon = QString(QChar(IconInfoCircle));
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

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        throwError(argumentError());
    else if ( !m_proxy->saveTab(m_tabName, getAbsoluteFilePath(filePath)) )
        throwSaveError(filePath);
}

void Scriptable::importTab()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        throwError(argumentError());
    else if ( !m_proxy->loadTab(getAbsoluteFilePath(filePath)) )
        throwImportError(filePath);
}

void Scriptable::importData()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        throwError(argumentError());
    else if ( !m_proxy->importData(getAbsoluteFilePath(filePath)) )
        throwImportError(filePath);
}

void Scriptable::exportData()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        throwError(argumentError());
    else if ( !m_proxy->exportData(getAbsoluteFilePath(filePath)) )
        throwSaveError(filePath);
}

QScriptValue Scriptable::config()
{
    m_skipArguments = -1;

    const auto nameValueInput = arguments();
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

        throwError(errors);
        return QScriptValue();
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

bool Scriptable::toggleConfig()
{
    m_skipArguments = 1;
    const auto optionName = arg(0);
    if ( optionName.isEmpty() )
        throwError(argumentError());

    const auto result = m_proxy->toggleConfig(optionName);
    if ( result.type() != QVariant::Bool ) {
        throwError( QString("Invalid boolean option \"%1\"!").arg(optionName) );
        return false;
    }

    return result.toBool();
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
#ifdef COPYQ_DESKTOP_FILE
                COPYQ_DESKTOP_FILE
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
        QFile scriptFile( getAbsoluteFilePath(scriptFilePath) );
        if ( !scriptFile.open(QIODevice::ReadOnly) ) {
            throwError( QString("Failed to open \"%1\": %2")
                        .arg(scriptFilePath,
                             scriptFile.errorString()) );
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

    if ( !getByteArray(m_input, this) )
        m_input = readInput();

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
    print(makeByteArray(value));
}

void Scriptable::abort()
{
    m_skipArguments = 0;
    abortEvaluation(m_action ? Abort::CurrentEvaluation : Abort::AllEvaluations);
}

void Scriptable::fail()
{
    m_skipArguments = 0;
    m_failed = true;
    abortEvaluation(Abort::CurrentEvaluation);
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

    QString expectedWidgetName;

    const auto focusPrefix = QLatin1String("focus:");
    for (int i = 0; i < argumentCount(); ++i) {
        const QString keys = toString(argument(i), this);

        if (keys.startsWith(focusPrefix)) {
            expectedWidgetName = keys.mid(focusPrefix.size());
            m_proxy->sendKeys(expectedWidgetName, QString(), 0);
        } else {
            waitFor(wait);
            m_proxy->sendKeys(expectedWidgetName, keys, delay);
        }

        // Make sure all keys are send (shortcuts are postponed because they can be blocked by modal windows).
        for (;;) {
            if ( m_proxy->sendKeysSucceeded() )
                break;

            if ( m_proxy->sendKeysFailed() ) {
                throwError("Failed to send key presses");
                return;
            }

            QCoreApplication::processEvents();
            if (!canContinue()) {
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
#endif // HAS_TESTS

void Scriptable::serverLog()
{
    m_skipArguments = 1;
    m_proxy->serverLog(arg(0));
}

QScriptValue Scriptable::logs()
{
    m_skipArguments = 0;
    return readLogFile(50 * 1024 * 1024);
}

void Scriptable::setCurrentTab()
{
    m_skipArguments = 1;
    const QString tabName = arg(0);
    m_proxy->setCurrentTab(tabName);
}

QScriptValue Scriptable::selectItems()
{
    const auto rows = getRows();
    m_skipArguments = rows.size();
    return m_proxy->selectItems(m_tabName, rows);
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
    const auto dataList = fromScriptValue<QVector<QVariantMap>>( argument(0), this );
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

    return toScriptValue( m_proxy->browserItemData(m_tabName, row), this );
}

void Scriptable::setItem()
{
    insert(2);
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

QScriptValue Scriptable::md5sum()
{
    return checksumForArgument(this, QCryptographicHash::Md5);
}

QScriptValue Scriptable::sha1sum()
{
    return checksumForArgument(this, QCryptographicHash::Sha1);
}

QScriptValue Scriptable::sha256sum()
{
    return checksumForArgument(this, QCryptographicHash::Sha256);
}

QScriptValue Scriptable::sha512sum()
{
    return checksumForArgument(this, QCryptographicHash::Sha512);
}

QScriptValue Scriptable::open()
{
    m_skipArguments = -1;
    return m_proxy->openUrls( arguments() );
}

QScriptValue Scriptable::execute()
{
    m_skipArguments = -1;

    m_executeStdoutData.clear();
    m_executeStdoutLastLine.clear();
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

    action.setCommand(args);
    action.setReadOutput(true);

    connect( &action, &Action::actionOutput,
             this, &Scriptable::onExecuteOutput );

    if ( !runAction(&action) || action.actionFailed() )
        return QScriptValue();

    if ( m_executeStdoutCallback.isFunction() ) {
        const auto arg = toScriptValue(m_executeStdoutLastLine, this);
        m_executeStdoutCallback.call( QScriptValue(), QScriptValueList() << arg );
    }

    QScriptValue actionResult = m_engine->newObject();
    actionResult.setProperty( "stdout", newByteArray(m_executeStdoutData) );
    actionResult.setProperty( "stderr", getTextData(action.errorOutput()) );
    actionResult.setProperty( "exit_code", action.exitCode() );

    m_executeStdoutData.clear();
    m_executeStdoutLastLine.clear();
    m_executeStdoutCallback = QScriptValue();

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

    const auto dialogId = m_proxy->inputDialog(values);
    if ( !canContinue() )
        return QScriptValue();

    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);
    connect( m_proxy, &ScriptableProxy::inputDialogFinished,
             &loop, [&](int finishedDialogId, const NamedValueList &result) {
                 if (finishedDialogId != dialogId)
                     return;
                 values = result;
                 loop.quit();
             });
    loop.exec();

    if (values.isEmpty())
        return QScriptValue();

    if (values.size() == 1)
        return toScriptValue( values.first().value, this );

    QScriptValue result = m_engine->newObject();

    for (const auto &value : values)
        result.setProperty( value.name, toScriptValue(value.value, this) );

    return result;
}

QScriptValue Scriptable::menuItems()
{
    const auto arg = argument(0);
    if ( arg.isString() ) {
        m_skipArguments = -1;
        QVector<QVariantMap> items;
        for (const auto &arg : arguments())
            items.append(createDataMap(mimeText, arg));
        const int i = m_proxy->menuItems(items);
        if (i == -1 || i >= items.size())
            return QString();
        return getTextData(items[i]);
    }

    m_skipArguments = 1;
    const auto items = fromScriptValue<QVector<QVariantMap>>( arg, this );
    if ( items.isEmpty() )
        return -1;
    return m_proxy->menuItems(items);
}

QScriptValue Scriptable::settings()
{
    m_skipArguments = 2;

    QSettings settings(
          QSettings::IniFormat,
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-scripts");

    if (argumentCount() == 2) {
        const QString key = arg(0);
        const QScriptValue value = argument(1);
        const QVariant saveValue = toVariant(value);
        settings.setValue(key, saveValue);
        return QScriptValue();
    }

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

QScriptValue Scriptable::commands()
{
    return toScriptValue( m_proxy->commands(), this );
}

void Scriptable::setCommands()
{
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), this);
    m_proxy->setCommands(commands);
}

void Scriptable::addCommands()
{
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), this);
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
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), this);

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

    int msec;
    if ( !toInt(argument(0), &msec) ) {
        throwError(argumentError());
        return;
    }

    if (canContinue()) {
        QEventLoop loop;
        connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);

        QTimer t;
        t.setTimerType(Qt::PreciseTimer);
        t.setInterval(msec);
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start();

        loop.exec();
    }
}

void Scriptable::afterMilliseconds()
{
    m_skipArguments = 2;

    if (argumentCount() < 2) {
        throwError(argumentError());
        return;
    }

    int msec;
    if ( !toInt(argument(0), &msec) ) {
        throwError(argumentError());
        return;
    }

    const auto fn = argument(1);
    if ( !fn.isFunction() ) {
        throwError(argumentError());
        return;
    }

    class TimedFunctionCall final : public QObject {
    public:
        TimedFunctionCall(int msec, const QScriptValue &fn, QObject *parent)
            : QObject(parent)
            , m_timerId( startTimer(msec, Qt::PreciseTimer) )
            , m_fn(fn)
        {
        }

    protected:
        void timerEvent(QTimerEvent *event) override
        {
            QObject::timerEvent(event);
            if ( m_timerId == event->timerId() ) {
                killTimer(m_timerId);
                m_fn.call();
            }
        }

    private:
        int m_timerId;
        QScriptValue m_fn;
    };

    new TimedFunctionCall(msec, fn, this);
}

QVariant Scriptable::call(const QString &method, const QVariantList &arguments)
{
    if ( m_engine->hasUncaughtException() )
        return QVariant();

    m_skipArguments = 2;

    auto fn = m_engine->globalObject().property(method);
    const auto result = call(&fn, QScriptValue(), arguments);
    if ( result.isUndefined() || result.isNull() || !result.isValid() )
        return QVariant();
    return toVariant(result);
}

QVariantList Scriptable::currentArguments()
{
    m_skipArguments = -1;
    QVariantList arguments;
    arguments.reserve( argumentCount() );
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

QScriptValue Scriptable::screenNames()
{
    return toScriptValue( m_proxy->screenNames(), this );
}

QScriptValue Scriptable::queryKeyboardModifiers()
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

QScriptValue Scriptable::pointerPosition()
{
    const QPoint pos = m_proxy->pointerPosition();
    return toScriptValue(QVector<int>{pos.x(), pos.y()}, this);
}

void Scriptable::setPointerPosition()
{
    m_skipArguments = 2;
    int x = 0;
    int y = 0;
    if ( !toInt(argument(0), &x) || !toInt(argument(1), &y) ) {
        throwError(argumentError());
        return;
    }
    m_proxy->setPointerPosition(x, y);
}

QScriptValue Scriptable::iconColor()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconColor();

    const auto color = arg(0);
    if ( !m_proxy->setIconColor(color) )
        throwError("Invalid color name");

    return QScriptValue();
}

QScriptValue Scriptable::iconTag()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconTag();

    const auto tag = arg(0);
    m_proxy->setIconTag(tag);

    return QScriptValue();
}

QScriptValue Scriptable::iconTagColor()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconTagColor();

    const auto color = arg(0);
    if ( !m_proxy->setIconTagColor(color) )
        throwError("Invalid color name");

    return QScriptValue();
}

void Scriptable::loadTheme()
{
    m_skipArguments = 1;

    const QString path = getAbsoluteFilePath(arg(0));
    const QString error = m_proxy->loadTheme(path);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::onClipboardChanged()
{
    eval(R"(
    if (!hasData()) {
        updateClipboardData();
    } else if (runAutomaticCommands()) {
        saveData();
        updateClipboardData();
    } else {
        clearClipboardData();
    }
    )");
}

void Scriptable::onOwnClipboardChanged()
{
    eval("updateClipboardData()");
}

void Scriptable::onHiddenClipboardChanged()
{
    eval("updateClipboardData()");
}

void Scriptable::onClipboardUnchanged()
{
}

void Scriptable::synchronizeToSelection()
{
    synchronizeSelection(ClipboardMode::Selection);
}

void Scriptable::synchronizeFromSelection()
{
    synchronizeSelection(ClipboardMode::Clipboard);
}

void Scriptable::setClipboardData()
{
    auto data = copyWithoutInternalData(m_data);
    m_proxy->setClipboardData(data);
}

void Scriptable::updateTitle()
{
    m_proxy->setTitleForData(m_data);
}

void Scriptable::setTitle()
{
    m_skipArguments = 1;
    const auto title = arg(0);
    m_proxy->setTitle(title);
}

void Scriptable::saveData()
{
    const QString outputTab = getTextData(m_data, mimeOutputTab);

    if ( !outputTab.isEmpty() )
        saveData(outputTab);
}

QScriptValue Scriptable::hasData()
{
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it) {
        const auto &format = it.key();
        if ( isInternalDataFormat(format) )
            continue;

        auto bytes = it.value().toByteArray();
        for (const auto &byte : bytes) {
            const QChar c(byte);
            if ( !c.isSpace() && !c.isNull() )
                return true;
        }
    }

    return false;
}

void Scriptable::showDataNotification()
{
    m_proxy->showDataNotification(m_data);
}

void Scriptable::hideDataNotification()
{
    m_proxy->showDataNotification(QVariantMap());
}

void Scriptable::updateClipboardData()
{
    eval(R"(
    if (isClipboard()) {
        updateTitle();
        showDataNotification();
        setClipboardData();
    }
    )");
}

void Scriptable::clearClipboardData()
{
    eval(R"(
    if (isClipboard()) {
        setTitle();
        hideDataNotification();
    }
    )");
}

QScriptValue Scriptable::runAutomaticCommands()
{
    return runCommands(CommandType::Automatic);
}

void Scriptable::runDisplayCommands()
{
    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, [&]() {
        if (m_abort == Abort::AllEvaluations)
            loop.exit();
    });

    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(0);
    connect(this, &Scriptable::dataReceived, &loop, [&](const QByteArray &receivedBytes) {
        if (receivedBytes == "ABORT") {
            abortEvaluation(Abort::AllEvaluations);
            return;
        }

        timer.start();
    });

    bool running = false;
    connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (running)
            return;
        running = true;

        if ( m_data.isEmpty() )
            m_data = m_proxy->setDisplayData(m_actionId, m_data);

        while ( !m_data.isEmpty() && runCommands(CommandType::Display) ) {
            m_data = m_proxy->setDisplayData(m_actionId, m_data);
        }

        m_data.clear();
        running = false;
    });

    emit receiveData();

    if (m_abort == Abort::None)
        loop.exec();
}

void Scriptable::runMenuCommandFilters()
{
    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, [&]() {
        if (m_abort == Abort::AllEvaluations)
            loop.exit();
    });

    QByteArray bytes;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(0);

    connect(this, &Scriptable::dataReceived, &loop, [&](const QByteArray &receivedBytes) {
        if (receivedBytes == "ABORT") {
            abortEvaluation(Abort::AllEvaluations);
            return;
        }

        bytes = receivedBytes;
        if ( !bytes.isEmpty() )
            timer.start();
    });

    // Avoid modifying menu filter data.
    const int actionId = m_actionId;
    m_actionId = -1;

    connect(&timer, &QTimer::timeout, &loop, [&]() {
        if ( bytes.isEmpty() )
            return;

        const int currentRun = bytes.toInt();

        getActionData(actionId);
        const QStringList matchCommands =
            m_data.value(COPYQ_MIME_PREFIX "match-commands").toStringList();

        PerformanceLogger logger( QLatin1String("Menu item filters") );

        for (int i = 0; i < matchCommands.length(); ++i) {
            m_engine->globalObject().setProperty( "menuItem", m_engine->newObject() );
            const bool enabled = canExecuteCommandFilter(matchCommands[i]);
            QVariantMap menuItem = toDataMap(m_engine->globalObject().property("menuItem"));
            menuItem["enabled"] = enabled && menuItem.value("enabled", true).toBool();
            if ( !m_proxy->enableMenuItem(actionId, currentRun, i, menuItem) )
                break;
        }
    });

    emit receiveData();

    if (m_abort == Abort::None)
        loop.exec();
}

void Scriptable::monitorClipboard()
{
    if (!verifyClipboardAccess())
        return;

    ClipboardMonitor monitor(
        fromScriptValue<QStringList>(eval("clipboardFormatsToSave()"), this) );

    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);
    connect( &monitor, &ClipboardMonitor::clipboardChanged,
             this, &Scriptable::onMonitorClipboardChanged );
    connect( &monitor, &ClipboardMonitor::clipboardUnchanged,
             this, &Scriptable::onMonitorClipboardUnchanged );
    connect( &monitor, &ClipboardMonitor::synchronizeSelection,
             this, &Scriptable::onSynchronizeSelection );
    loop.exec();
}

void Scriptable::provideClipboard()
{
    provideClipboard(ClipboardMode::Clipboard);
}

void Scriptable::provideSelection()
{
    provideClipboard(ClipboardMode::Selection);
}

QScriptValue Scriptable::clipboardFormatsToSave()
{
    ItemFactory factory;
    factory.loadPlugins();

    QSettings settings;
    factory.loadItemFactorySettings(&settings);

    QStringList formats = factory.formatsToSave();
    COPYQ_LOG( "Clipboard formats to save: " + formats.join(", ") );

    for (const auto &command : m_proxy->automaticCommands()) {
        if ( !command.input.isEmpty() && !formats.contains(command.input) ) {
            COPYQ_LOG( QString("Clipboard format to save for command \"%1\": %2")
                       .arg(command.name, command.input) );
            formats.append(command.input);
        }
    }

    return toScriptValue(formats, this);
}

void Scriptable::onExecuteOutput(const QByteArray &output)
{
    m_executeStdoutData.append(output);

    if ( m_executeStdoutCallback.isFunction() ) {
        m_executeStdoutLastLine.append( getTextData(output) );
        auto lines = m_executeStdoutLastLine.split('\n');
        m_executeStdoutLastLine = lines.takeLast();
        if ( !lines.isEmpty() ) {
            const auto arg = toScriptValue(lines, this);
            m_executeStdoutCallback.call( QScriptValue(), QScriptValueList() << arg );
        }
    }
}

void Scriptable::onMonitorClipboardChanged(const QVariantMap &data, ClipboardOwnership ownership)
{
    COPYQ_LOG( QString("onMonitorClipboardChanged: %1 %2, owner is \"%3\"")
               .arg(ownership == ClipboardOwnership::Own ? "own"
                  : ownership == ClipboardOwnership::Foreign ? "foreign"
                  : "hidden")
               .arg(isClipboardData(data) ? "clipboard" : "selection")
               .arg(getTextData(data, mimeOwner)) );

    const QString command =
        ownership == ClipboardOwnership::Own ? "copyq onOwnClipboardChanged"
      : ownership == ClipboardOwnership::Hidden ? "copyq onHiddenClipboardChanged"
      : "copyq onClipboardChanged";

    m_proxy->runInternalAction(data, command);
}

void Scriptable::onMonitorClipboardUnchanged(const QVariantMap &data)
{
    m_proxy->runInternalAction(data, "copyq onClipboardUnchanged");
}

void Scriptable::onSynchronizeSelection(ClipboardMode sourceMode, const QString &text, uint targetTextHash)
{
#ifdef HAS_MOUSE_SELECTIONS
    auto data = createDataMap(mimeText, text);
    data[COPYQ_MIME_PREFIX "target-text-hash"] = QByteArray::number(targetTextHash);
    const auto command = sourceMode == ClipboardMode::Clipboard
        ? "copyq --clipboard-access synchronizeToSelection"
        : "copyq --clipboard-access synchronizeFromSelection";
    m_proxy->runInternalAction(data, command);
#else
    Q_UNUSED(text);
    Q_UNUSED(sourceMode);
#endif
}

bool Scriptable::sourceScriptCommands()
{
    const auto commands = m_proxy->scriptCommands();
    for (const auto &command : commands) {
        engine()->pushContext();
        eval(command.cmd, command.name);
        engine()->popContext();
        if ( engine()->hasUncaughtException() ) {
            const auto exceptionText = processUncaughtException("ScriptCommand::" + command.name);
            const auto message = createScriptErrorMessage(exceptionText).toUtf8();
            printError(message);
            return false;
        }
    }

    return true;
}

int Scriptable::executeArguments(const QStringList &args)
{
    if ( args.isEmpty() ) {
        logScriptError("Bad command syntax");
        return CommandBadSyntax;
    }

    if ( !sourceScriptCommands() )
        return CommandError;

    /* Special arguments:
     * "-"  read this argument from stdin
     * "--" read all following arguments without control sequences
     */
    QScriptValueList fnArgs;
    bool readRaw = false;
    for (const auto &arg : args) {
        if (readRaw) {
            fnArgs.append( newByteArray(arg.toUtf8()) );
        } else if (arg == "--") {
            readRaw = true;
        } else if (arg == "-") {
            fnArgs.append( input() );
        } else if (arg == "-e") {
            fnArgs.append("eval");
        } else {
            const auto unescapedArg = parseCommandLineArgument(arg);
            const auto value = newByteArray( unescapedArg.toUtf8() );
            fnArgs.append(value);
        }
    }

    QString cmd;
    QScriptValue result;

    int skipArguments = 0;
    if (!fnArgs.isEmpty() && toString(fnArgs[0], this) == "--clipboard-access")
        ++skipArguments;

    while ( skipArguments < fnArgs.size() && canContinue() && !m_engine->hasUncaughtException() ) {
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

    if ( result.isFunction() && canContinue() && !m_engine->hasUncaughtException() )
        result = result.call( result.data(), fnArgs.mid(skipArguments) );

    int exitCode;

    if (m_failed) {
        exitCode = CommandError;
    } else if (m_abort != Abort::None) {
        exitCode = CommandFinished;
    } else if ( m_engine->hasUncaughtException() ) {
        const auto exceptionText = processUncaughtException(QString());
        const auto message = createScriptErrorMessage(exceptionText).toUtf8();
        printError(message);
        exitCode = CommandException;
    } else {
        const auto message = serializeScriptValue(result);
        print(message);
        exitCode = CommandFinished;
    }

    if (exitCode == CommandFinished)
        setActionData();

    // Destroy objects so destructors are run before script finishes
    // (e.g. file writes are flushed or temporary files are automatically removed).
    m_engine->collectGarbage();

    return exitCode;
}

QString Scriptable::processUncaughtException(const QString &cmd)
{
    if ( !m_engine->hasUncaughtException() )
        return QString();

    const auto exceptionName = m_engine->uncaughtException().toString()
            .remove(QRegularExpression("^Error: "))
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

    auto label = m_actionName;
    if ( !cmd.isEmpty() )
        label.append("::" + cmd);

    if (label.isEmpty()) {
        logScriptError(
            QString("Exception in command: %2")
            .arg(exceptionText) );
    } else {
        logScriptError(
            QString("Exception in command \"%1\": %2")
            .arg(label, exceptionText) );
    }

    // Show exception popups only if the script was launched from application.
    // (avoid it if launched from command line).
    if (m_actionId != -1)
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

    QtPrivate::QHashCombine hash;
    const auto id = hash(hash(0, title), message);
    const auto notificationId = QString::number(id);
    m_proxy->showMessage(title, message, QString(QChar(IconExclamationCircle)), 8000, notificationId);
}

QVector<int> Scriptable::getRows() const
{
    QVector<int> rows;

    for ( int i = 0; i < argumentCount(); ++i ) {
        int row;
        if ( !toInt(argument(i), &row) )
            break;
        rows.append(row);
    }

    return rows;
}

QScriptValue Scriptable::copy(ClipboardMode mode)
{
    const int args = argumentCount();
    QVariantMap data;

    if (args == 0) {
        // Reset clipboard first.
        const QString mime = COPYQ_MIME_PREFIX "invalid";
        const QByteArray value = "invalid";
        data.insert(mime, value);
        m_proxy->setClipboard(data, mode);

        m_proxy->copyFromCurrentWindow();

        // Wait for clipboard to be set.
        for (int i = 0; i < 10; ++i) {
            if ( getClipboardData(mime) != value )
                return true;
            waitFor(5 + i * 25);
        }

        throwError( tr("Failed to copy to clipboard!") );
        return false;
    }

    if (args == 1) {
        QScriptValue value = argument(0);
        setTextData( &data, toString(value, this) );
        m_proxy->setClipboard(data, mode);
        return true;
    }

    if (args % 2 == 0) {
        for (int i = 0; i < args; ++i) {
            // MIME
            QString mime = toString(argument(i), this);

            // DATA
            toItemData(argument(++i), mime, &data);
        }

        m_proxy->setClipboard(data, mode);
        return true;
    }

    throwError(argumentError());
    return false;
}

void Scriptable::abortEvaluation(Abort abort)
{
    m_abort = abort;
    throwError("Evaluation aborted");
    emit finished();
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
        QVector<QVariantMap> items(1, data);
        const auto error = m_proxy->browserInsert(m_tabName, row, items);
        if ( !error.isEmpty() )
            throwError(error);
    } else {
        m_proxy->browserChange(m_tabName, data, row);
    }
}

void Scriptable::nextToClipboard(int where)
{
    QVariantMap data = m_proxy->nextItem(m_tabName, where);
    if (data.isEmpty())
        return;

    m_proxy->setClipboard(data, ClipboardMode::Clipboard);
#ifdef HAS_MOUSE_SELECTIONS
    m_proxy->setClipboard(data, ClipboardMode::Selection);
#endif
}

QScriptValue Scriptable::screenshot(bool select)
{
    m_skipArguments = 2;

    const auto format = arg(0, "png");
    const auto screen = arg(1);
    const auto imageData = m_proxy->screenshot(format, screen, select);

    if ( imageData.isEmpty() ) {
        QString error = "Failed to grab screenshot";
        if ( !screen.isEmpty() ) {
            const auto screenNames = m_proxy->screenNames();
            error.append( " (valid screen names are " + screenNames.join(", ") + ")" );
        }
        throwError(error);
        return QScriptValue();
    }

    return newByteArray(imageData);
}

QScriptValue Scriptable::eval(const QString &script, const QString &fileName)
{
    const auto syntaxResult = QScriptEngine::checkSyntax(script);
    if (syntaxResult.state() != QScriptSyntaxCheckResult::Valid) {
        throwError( QString("%1:%2:%3: syntax error: %4")
                    .arg(fileName)
                    .arg(syntaxResult.errorLineNumber())
                    .arg(syntaxResult.errorColumnNumber())
                    .arg(syntaxResult.errorMessage()) );
        return QScriptValue();
    }

    const auto result = engine()->evaluate(script, fileName);

    if (m_abort != Abort::None) {
        engine()->clearExceptions();
        if (m_abort == Abort::AllEvaluations)
            abortEvaluation(Abort::AllEvaluations);
        else
            m_abort = Abort::None;

        return QScriptValue();
    }

    return result;
}

void Scriptable::setActionId(int actionId)
{
    m_actionId = actionId;
    getActionData();
}

void Scriptable::setActionName(const QString &actionName)
{
    m_actionName = actionName;
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

bool Scriptable::runAction(Action *action)
{
    if (!canContinue())
        return false;

    // Shortcut to run script in current Scriptable
    // instead of spawning new process.
    const auto &cmd = action->command();
    const auto cmd1 = cmd.value(0).value(0);
    if ( cmd.size() == 1 && cmd[0].size() == 1
         && cmd1.size() >= 2
         && cmd1[0] == "copyq"
         && (!cmd1[1].startsWith("-") || cmd1[1] == "-e") )
    {
        const auto oldInput = m_input;
        m_input = newByteArray(action->input());

        const auto oldAction = m_action;
        m_action = action;
        engine()->pushContext();

        const auto exitCode = executeArguments(cmd1.mid(1));
        action->setExitCode(exitCode);
        m_failed = false;
        m_engine->clearExceptions();
        if (m_abort == Abort::AllEvaluations)
            abortEvaluation(Abort::AllEvaluations);
        else
            m_abort = Abort::None;

        engine()->popContext();
        m_action = oldAction;
        m_input = oldInput;

        return true;
    }

    // Update data for the new action.
    setActionData();

    action->setWorkingDirectory( m_dirClass->getCurrentPath() );
    action->start();

    while ( !action->waitForFinished(5000) && canContinue() ) {}

    if ( action->isRunning() && !action->waitForFinished(5000) ) {
        action->terminate();
        return false;
    }

    // Data could have changed so refetch them.
    getActionData();

    return true;
}

bool Scriptable::runCommands(CommandType::CommandType type)
{
    Q_ASSERT(type == CommandType::Automatic || type == CommandType::Display);

    const auto label = type == CommandType::Automatic
            ? "Automatic command \"%1\": %2"
            : "Display command \"%1\": %2";

    auto commands = type == CommandType::Automatic
            ? m_proxy->automaticCommands()
            : m_proxy->displayCommands();
    const QString tabName = getTextData(m_data, mimeCurrentTab);

    for (auto &command : commands) {
        PerformanceLogger logger( QString("Command \"%1\"").arg(command.name) );

        if ( command.outputTab.isEmpty() )
            command.outputTab = tabName;

        if ( !canExecuteCommand(command) )
            continue;

        if ( canContinue() && !command.cmd.isEmpty() ) {
            Action action;
            action.setCommand( command.cmd, QStringList(getTextData(m_data)) );
            action.setInputWithFormat(m_data, command.input);
            action.setName(command.name);
            action.setData(m_data);

            // FIXME: Handle automatic and display command output.
            //action.setOutputFormat(command.output);
            //action.setItemSeparator(QRegularExpression(command.sep));
            //action.setOutputTab(command.outputTab);

            if ( !runAction(&action) && canContinue() ) {
                throwError( QString(label).arg(command.name, "Failed to start") );
                return false;
            }
        }

        if (!canContinue()) {
            COPYQ_LOG( QString(label).arg(command.name, "Interrupted") );
            return false;
        }

        if ( type == CommandType::Automatic ) {
            if ( !command.tab.isEmpty() ) {
                COPYQ_LOG( QString(label).arg(command.name, "Saving to tab: " + command.tab) );
                saveData(command.tab);
            }

            if ( command.remove || command.transform || m_data.contains(mimeIgnore) ) {
                COPYQ_LOG( QString(label).arg(command.name, "Ignoring data") );
                return false;
            }
        }

        COPYQ_LOG_VERBOSE( QString(label).arg(command.name, "Finished") );
    }

    return true;
}

bool Scriptable::canExecuteCommand(const Command &command)
{
    // Verify that data for given MIME is available.
    if ( !command.input.isEmpty() ) {
        if (command.input == mimeItems || command.input == "!OUTPUT") {
            // Disallow applying action that takes serialized item more times.
            if ( m_data.contains(command.output) )
                return false;
        } else if ( !m_data.contains(command.input) ) {
            return false;
        }
    }

    // Verify that and text matches given regexp.
    if ( !matchData(command.re, m_data, mimeText) )
        return false;

    // Verify that window title matches given regexp.
    if ( !matchData(command.wndre, m_data, mimeWindowTitle) )
        return false;

    return canExecuteCommandFilter(command.matchCmd);
}

bool Scriptable::canExecuteCommandFilter(const QString &matchCommand)
{
    if ( matchCommand.isEmpty() )
        return true;

    Action action;

    const QString text = getTextData(m_data);
    action.setInput(text.toUtf8());
    action.setData(m_data);

    const QString arg = getTextData(action.input());
    action.setCommand(matchCommand, QStringList(arg));

    return runAction(&action) && action.exitCode() == 0;
}

bool Scriptable::canAccessClipboard() const
{
    return qobject_cast<QGuiApplication*>(qApp);
}

bool Scriptable::verifyClipboardAccess()
{
    if ( canAccessClipboard() )
        return true;

    throwError("Cannot access system clipboard with QCoreApplication");
    return false;
}

void Scriptable::provideClipboard(ClipboardMode mode)
{
    if (!verifyClipboardAccess())
        return;

    const auto owner = makeClipboardOwnerData();
    m_data.insert(mimeOwner, owner);

    platformNativeInterface()->clipboard()->setData(mode, m_data);

    QEventLoop loop;

    const auto checkClipboardOwnership = [&]() {
        if ( clipboardOwnerData(mode) != owner )
            loop.quit();
    };

    QTimer t;
    t.setInterval(8000);
    connect(&t, &QTimer::timeout, this, checkClipboardOwnership);
    t.start();

    connect( this, &Scriptable::finished, &loop, &QEventLoop::quit );
    connect( QGuiApplication::clipboard(), &QClipboard::changed,
             &t, checkClipboardOwnership );

    COPYQ_LOG( QString("Started providing %1")
               .arg(mode == ClipboardMode::Clipboard ? "clipboard" : "selection") );

    loop.exec();

    COPYQ_LOG( QString("Finished providing %1")
               .arg(mode == ClipboardMode::Clipboard ? "clipboard" : "selection") );
}

void Scriptable::insert(int argumentsEnd)
{
    int row;
    if ( !toInt(argument(0), &row) ) {
        throwError(argumentError());
        return;
    }

    insert(row, 1, argumentsEnd);
}

void Scriptable::insert(int row, int argumentsBegin, int argumentsEnd)
{
    m_skipArguments = argumentsEnd;

    QVector<QVariantMap> items;
    items.reserve(argumentsEnd - argumentsBegin);

    for (int i = argumentsBegin; i < argumentsEnd; ++i) {
        const auto arg = argument(i);
        if ( arg.isObject() && arg.scriptClass() != byteArrayClass() && !arg.isArray() )
            items.append( fromScriptValue<QVariantMap>(arg, this) );
        else
            items.append( createDataMap(mimeText, toString(arg, this)) );
    }

    const auto error = m_proxy->browserInsert(m_tabName, row, items);
    if ( !error.isEmpty() )
        throwError(error);
}

QStringList Scriptable::arguments()
{
    QStringList args;
    args.reserve( argumentCount() );

    for (int i = 0; i < argumentCount(); ++i)
        args.append( arg(i) );

    return args;
}

void Scriptable::print(const QByteArray &message)
{
    if (m_action) {
        m_action->appendOutput(message);
    } else {
        QFile f;
        f.open(stdout, QIODevice::WriteOnly);
        f.write(message);
    }
}

void Scriptable::printError(const QByteArray &message)
{
    if (m_action) {
        m_action->appendErrorOutput(message);
    } else {
        QFile f;
        f.open(stderr, QIODevice::WriteOnly);
        f.write(message);
        if ( !message.endsWith('\n') )
            f.write("\n");
    }
}

void Scriptable::getActionData()
{
    getActionData(m_actionId);
}

void Scriptable::getActionData(int actionId)
{
    if (actionId != -1)
        m_data = m_oldData = m_proxy->getActionData(actionId);
}

void Scriptable::setActionData()
{
    if (m_actionId != -1 && m_oldData != m_data) {
        m_proxy->setActionData(m_actionId, m_data);
        m_oldData = m_data;
    }
}

QByteArray Scriptable::getClipboardData(const QString &mime, ClipboardMode mode)
{
    if ( !canAccessClipboard() ) {
        const auto command = QString("copyq --clipboard-access %1 %2")
                .arg(mode == ClipboardMode::Clipboard ? "clipboard" : "selection")
                .arg(mime);
        return m_proxy->tryGetCommandOutput(command);
    }

    const QMimeData *data = clipboardData(mode);
    if (!data)
        return QByteArray();

    if (mime == "?")
        return data->formats().join("\n").toUtf8() + '\n';

    return cloneData(*data, QStringList(mime)).value(mime).toByteArray();
}

bool Scriptable::hasClipboardFormat(const QString &mime, ClipboardMode mode)
{
    if ( !canAccessClipboard() ) {
        const auto command = QString("copyq --clipboard-access %1 %2")
                .arg(mode == ClipboardMode::Clipboard ? "hasClipboardFormat" : "hasSelectionFormat")
                .arg(mime);
        return m_proxy->tryGetCommandOutput(command).startsWith("true");
    }

    const QMimeData *data = clipboardData(mode);
    return data && data->hasFormat(mime);
}

void Scriptable::synchronizeSelection(ClipboardMode targetMode)
{
#ifdef HAS_MOUSE_SELECTIONS
#   define COPYQ_SYNC_LOG(MESSAGE) \
        COPYQ_LOG( QString("Synchronizing to %1: " MESSAGE) \
                   .arg(targetMode == ClipboardMode::Clipboard ? "clipboard" : "selection") );

    if (!verifyClipboardAccess())
        return;

    {
        SleepTimer tMin(50);

        // Avoid changing clipboard after a text is selected just before it's copied
        // with a keyboard shortcut.
        SleepTimer t(5000);
        while ( QGuiApplication::queryKeyboardModifiers() != Qt::NoModifier ) {
            if ( !t.sleep() && !canContinue() )
                return;
        }

        const auto sourceMode = targetMode == ClipboardMode::Selection ? ClipboardMode::Clipboard : ClipboardMode::Selection;

        // Wait at least few milliseconds before synchronization
        // to avoid overriding just changed clipboard/selection.
        while ( tMin.sleep() ) {}

        // Stop if the clipboard/selection text already changed again.
        const auto sourceData = clipboardData(sourceMode);
        if (!sourceData)
            return;
        const QString sourceText = cloneText(*sourceData);
        if (sourceText != getTextData(m_data)) {
            COPYQ_SYNC_LOG("Cancelled (source text changed)");
            return;
        }

        const auto targetData = clipboardData(targetMode);
        if (!targetData)
            return;
        const QString targetText = cloneText(*targetData);
        if ( clipboardOwnerData(targetMode).isEmpty() && !targetText.isEmpty() ) {
            const auto targetTextHash = m_data.value(COPYQ_MIME_PREFIX "target-text-hash").toByteArray().toUInt();
            if (targetTextHash != qHash(targetText)) {
                COPYQ_SYNC_LOG("Cancelled (target text changed)");
                return;
            }
        }

        // Stop if the clipboard and selection text is already synchronized
        // or user selected text and copied it to clipboard.
        if (sourceText == targetText) {
            COPYQ_SYNC_LOG("Cancelled (target is same as source)");
            return;
        }
    }

    COPYQ_SYNC_LOG("Calling provideClipboard()/provideSelection()");

    if (targetMode == ClipboardMode::Clipboard)
        provideClipboard();
    else
        provideSelection();
#   undef COPYQ_SYNC_LOG
#else
    Q_UNUSED(targetMode);
#endif
}

void Scriptable::saveData(const QString &tab)
{
    const auto data = copyWithoutInternalData(m_data);
    const auto clipboardMode = isClipboardData(m_data)
            ? ClipboardMode::Clipboard
            : ClipboardMode::Selection;
    m_proxy->saveData(tab, data, clipboardMode);
}

QScriptValue Scriptable::readInput()
{
    // Try to read stdin in a non-blocking way.
    class InputReaderThread final : public QThread {
    public:
        QByteArray input;

    protected:
        void run() override {
            QFile in;
            in.open(stdin, QIODevice::ReadOnly);
            input = in.readAll();
        }
    };

    auto inputReaderThread = new InputReaderThread;
    QEventLoop loop;
    connect(inputReaderThread, &QThread::finished, &loop, &QEventLoop::quit);
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);
    inputReaderThread->start();
    loop.exec();

    if ( inputReaderThread->isRunning() ) {
        inputReaderThread->terminate();
        COPYQ_LOG("Terminating input reader");
        if ( !inputReaderThread->wait(4000) )
            COPYQ_LOG("Failed to terminate input reader");
        return QScriptValue();
    }

    inputReaderThread->deleteLater();
    return newByteArray(inputReaderThread->input);
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

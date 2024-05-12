// SPDX-License-Identifier: GPL-3.0-or-later

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
#include "gui/clipboardspy.h"
#include "gui/icons.h"
#include "item/itemfactory.h"
#include "item/serialize.h"
#include "platform/platformclipboard.h"
#include "platform/platformwindow.h"
#include "scriptable/commandhelp.h"
#include "scriptable/scriptablebytearray.h"
#include "scriptable/scriptabledir.h"
#include "scriptable/scriptablefile.h"
#include "scriptable/scriptableitemselection.h"
#include "scriptable/scriptableproxy.h"
#include "scriptable/scriptablesettings.h"
#include "scriptable/scriptabletemporaryfile.h"
#include "scriptable/scriptoverrides.h"
#include "scriptable/scriptvaluefactory.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMimeData>
#include <QMetaProperty>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPoint>
#include <QRegularExpression>
#include <QJSEngine>
#include <QJSValueIterator>
#include <QSettings>
#include <QSysInfo>
#include <QUrl>
#include <QVector>
#include <QThread>
#include <QTimer>

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#   include <QTextCodec>
#else
#   include <QStringDecoder>
#   include <QStringEncoder>
#endif

#ifdef WITH_NATIVE_NOTIFICATIONS
#   include <knotifications_version.h>
#endif

Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

namespace {

const QLatin1String mimeIgnore(COPYQ_MIME_PREFIX "ignore");

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

        ::log( QStringLiteral("%1: %2")
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

QString exceptionBacktrace(const QJSValue &exception, const QStringList &stack = {})
{
    const auto backtraceValue = exception.property("stack");
    auto backtrace = backtraceValue.isUndefined() ? QStringList() : backtraceValue.toString().split("\n");
    for (int i = backtrace.size() - 1; i >= 0; --i) {
        if ( backtrace[i] == QLatin1String("@:1") )
            backtrace.removeAt(i);
    }
    for (const auto &frame : stack)
        backtrace.append(frame);

    if ( backtrace.isEmpty() )
        return {};

    return QStringLiteral("\n\n--- backtrace ---\n%1\n--- end backtrace ---")
        .arg(backtrace.join(QLatin1String("\n")));

}

QJSValue evaluateStrict(QJSEngine *engine, const QString &script)
{
    const auto v = engine->evaluate(script);
    if ( v.isError() ) {
        const auto scriptText = QStringLiteral("--- SCRIPT BEGIN ---\n%1\n--- SCRIPT END ---").arg(script);
        log( QStringLiteral("Exception during evaluate: %1%2\n\n%3")
            .arg(v.toString(), exceptionBacktrace(v), scriptText), LogError );
    }
    return v;
}

QJSValue addScriptableClass(const QMetaObject *metaObject, const QString &name, QJSEngine *engine)
{
    auto cls = engine->newQMetaObject(metaObject);
    auto fn = engine->globalObject().property(name);
    Q_ASSERT(fn.isCallable());
    fn.setProperty(QStringLiteral("prototype"), cls);
    return cls;
}

QByteArray serializeScriptValue(const QJSValue &value, Scriptable *scriptable)
{
    QByteArray data;

    const QByteArray *bytes = getByteArray(value);

    if (bytes != nullptr) {
        data = *bytes;
    } else if ( value.isArray() || value.toVariant().type() == QVariant::StringList ) {
        const quint32 len = value.property("length").toUInt();
        for (quint32 i = 0; i < len; ++i)
            data += serializeScriptValue(value.property(i), scriptable);
    } else if ( !value.isUndefined() ) {
        data = value.toString().toUtf8() + '\n';
    }

    return data;
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

QJSValue checksumForArgument(Scriptable *scriptable, QCryptographicHash::Algorithm method)
{
    const auto data = scriptable->makeByteArray(scriptable->argument(0));
    const QByteArray hash = QCryptographicHash::hash(data, method).toHex();
    return QLatin1String(hash);
}

QString scriptToLabel(const QString &script)
{
    constexpr auto maxScriptSize = 30;
    if (maxScriptSize < script.size())
        return script.left(maxScriptSize).simplified() + QLatin1String("...");
    return script;
}

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
QTextCodec *codecFromNameOrThrow(const QJSValue &codecName, Scriptable *scriptable)
{
    const auto codec = QTextCodec::codecForName( scriptable->makeByteArray(codecName) );
    if (!codec) {
        QString codecs;
        for (const auto &availableCodecName : QTextCodec::availableCodecs())
            codecs.append( "\n" + QLatin1String(availableCodecName) );
        scriptable->throwError("Available codecs are:" + codecs);
    }
    return codec;
}

QJSValue toUnicode(const QByteArray &bytes, const QJSValue &codecName, Scriptable *scriptable)
{
    const auto codec = codecFromNameOrThrow(codecName, scriptable);
    if (!codec)
        return QJSValue();

    return codec->toUnicode(bytes);
}

QJSValue toUnicode(const QByteArray &bytes, Scriptable *scriptable)
{
    const auto codec = QTextCodec::codecForUtfText(bytes, nullptr);
    if (!codec)
        return scriptable->throwError("Failed to detect encoding");
    return codec->toUnicode(bytes);
}

QJSValue fromUnicode(const QString &text, const QJSValue &codecName, Scriptable *scriptable)
{
    const auto codec = codecFromNameOrThrow(codecName, scriptable);
    if (!codec)
        return QJSValue();

    return scriptable->newByteArray( codec->fromUnicode(text) );
}
#else
std::optional<QStringConverter::Encoding> encodingFromNameOrThrow(const QJSValue &codecName, Scriptable *scriptable)
{
    const auto encoding = QStringConverter::encodingForName( scriptable->makeByteArray(codecName) );
    if (!encoding)
        scriptable->throwError("Unknown encoding name");
    return encoding;
}

QJSValue toUnicode(const QByteArray &bytes, const QJSValue &codecName, Scriptable *scriptable)
{
    const auto encoding = encodingFromNameOrThrow(codecName, scriptable);
    if (!encoding)
        return QJSValue();
    const QString text = QStringDecoder(*encoding).decode(bytes);
    return text;
}

QJSValue toUnicode(const QByteArray &bytes, Scriptable *scriptable)
{
    const auto encoding = QStringConverter::encodingForData(bytes);
    if (!encoding)
        return scriptable->throwError("Failed to detect encoding");
    const QString text = QStringDecoder(*encoding).decode(bytes);
    return text;
}

QJSValue fromUnicode(const QString &text, const QJSValue &codecName, Scriptable *scriptable)
{
    const auto encoding = encodingFromNameOrThrow(codecName, scriptable);
    if (!encoding)
        return QJSValue();
    const QStringConverter::Flags flags = *encoding == QStringConverter::Utf8
        ? QStringConverter::Flag::Default
        : (QStringConverter::Flag::Default | QStringConverter::Flag::WriteBom);
    return scriptable->newByteArray(
        QStringEncoder(*encoding, flags).encode(text) );
}
#endif

bool isGuiApplication()
{
    return qobject_cast<QGuiApplication*>(qApp);
}

bool isOverridden(const QJSValue &globalObject, const QString &property)
{
    return globalObject.property(property).property(QStringLiteral("_copyq")).toString() != property;
}

} // namespace

Scriptable::Scriptable(
        QJSEngine *engine,
        ScriptableProxy *proxy,
        ItemFactory *factory,
        QObject *parent)
    : QObject(parent)
    , m_proxy(proxy)
    , m_engine(engine)
    , m_factory(factory)
    , m_inputSeparator("\n")
    , m_input()
{
    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    QJSValue globalObject = m_engine->globalObject();
    globalObject.setProperty(QStringLiteral("global"), globalObject);

    m_safeCall = evaluateStrict(m_engine, QStringLiteral(
        "(function() {"
            "try {return this.apply(global, arguments);}"
            "catch(e) {_copyqUncaughtException = e; throw e;}"
        "})"
    ));

    m_safeEval = evaluateStrict(m_engine, QStringLiteral(
        "(function(){"
            "var _eval = eval;"
            "return function(script) {"
                "try {return _eval(script);}"
                "catch(e) {_copyqUncaughtException = e; throw e;}"
            "}"
        "})()"
    ));

    m_createFn = evaluateStrict(m_engine, QStringLiteral(
        "(function(from, name) {"
            "var f = function() {"
                "_copyqArguments = arguments;"
                "var v = from[name]();"
                "_copyqArguments = null;"
                "if (_copyqHasUncaughtException) throw _copyqUncaughtException;"
                "return v;"
            "};"
            "f._copyq = name;"
            "return f;"
        "})"
    ));
    m_createFnB = evaluateStrict(m_engine, QStringLiteral(
        "(function(from, name) {"
            "var f = function() {"
                "_copyqArguments = arguments;"
                "var v = from[name]();"
                "_copyqArguments = null;"
                "if (_copyqHasUncaughtException) throw _copyqUncaughtException;"
                "return ByteArray(v);"
            "};"
            "f._copyq = 1;"
            "return f;"
        "})"
    ));

    m_createProperty = evaluateStrict(m_engine, QStringLiteral(
        "(function(obj, name, from) {"
            "Object.defineProperty(obj, name, {"
            "get: function(){return from[name];},"
            "set: function(arg){from[name] = arg},"
            "});"
        "})"
    ));

    installObject(this, &Scriptable::staticMetaObject, globalObject);

    m_byteArrayPrototype = addScriptableClass(
        &ScriptableByteArray::staticMetaObject, QStringLiteral("ByteArray"), m_engine);
    m_filePrototype = addScriptableClass(
        &ScriptableFile::staticMetaObject, QStringLiteral("File"), m_engine);
    m_temporaryFilePrototype = addScriptableClass(
        &ScriptableTemporaryFile::staticMetaObject, QStringLiteral("TemporaryFile"), m_engine);
    m_dirPrototype = addScriptableClass(
        &ScriptableDir::staticMetaObject, QStringLiteral("Dir"), m_engine);
    m_itemSelectionPrototype = addScriptableClass(
        &ScriptableItemSelection::staticMetaObject, QStringLiteral("ItemSelection"), m_engine);
    m_settingsPrototype = addScriptableClass(
        &ScriptableSettings::staticMetaObject, QStringLiteral("Settings"), m_engine);
}

QJSValue Scriptable::argumentsArray() const
{
    return m_engine->globalObject().property("_copyqArguments");
}

int Scriptable::argumentCount() const
{
    return argumentsArray().property("length").toInt();
}

QJSValue Scriptable::argument(int index) const
{
    return argumentsArray().property(static_cast<quint32>(index) );
}

QJSValue Scriptable::newByteArray(const QByteArray &bytes) const
{
    return newQObject(new ScriptableByteArray(bytes), m_byteArrayPrototype);
}

QJSValue Scriptable::newByteArray(ScriptableByteArray *ba) const
{
    return newQObject(ba, m_byteArrayPrototype);
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
    return fromScriptValue<QVariant>(value, m_engine);
}

bool Scriptable::toInt(const QJSValue &value, int *number) const
{
    bool ok;
    *number = toString(value).toInt(&ok);
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
    return toScriptValue(dataMap, m_engine);
}

QByteArray Scriptable::makeByteArray(const QJSValue &value) const
{
    const QByteArray *data = getByteArray(value);
    if (data)
        return *data;

    const QVariant variant = value.toVariant();
    if (variant.type() == QVariant::ByteArray)
        return variant.toByteArray();

    return fromString(value.toString());
}

bool Scriptable::toItemData(const QJSValue &value, const QString &mime, QVariantMap *data) const
{
    if (value.isUndefined()) {
        data->insert( mime, QVariant() );
        return true;
    }

    const QByteArray *itemData = getByteArray(value);
    if (mime == mimeItems)
        return itemData && deserializeData(data, *itemData);

    data->insert( mime, itemData ? *itemData : toString(value).toUtf8() );
    return true;
}

QJSValue Scriptable::getInputSeparator() const
{
    return m_inputSeparator;
}

void Scriptable::setInputSeparator(const QJSValue &separator)
{
    m_inputSeparator = toString(separator);
}

QString Scriptable::getCurrentPath() const
{
    return QDir::currentPath();
}

void Scriptable::setCurrentPath(const QString &path)
{
    QDir::setCurrent(path);
}

QString Scriptable::getAbsoluteFilePath(const QString &fileName) const
{
    return QDir::isRelativePath(fileName) ? getCurrentPath() + '/' + fileName
                                          : fileName;
}

QString Scriptable::arg(int i, const QString &defaultValue)
{
    return i < argumentCount() ? toString(argument(i)) : defaultValue;
}

QJSValue Scriptable::throwError(const QString &errorMessage)
{
    // QJSEngine::throwError() is available in Qt 5.12.
    QJSValue throwFn = evaluateStrict(m_engine,  QStringLiteral(
        "(function(text) {throw new Error(text);})"
    ));
    const auto exc = throwFn.call({errorMessage});
#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
    m_engine->throwError(QJSValue::GenericError, errorMessage);
    return exc;
#else
    setUncaughtException(exc);
    return m_uncaughtException;
#endif
}

QJSValue Scriptable::throwSaveError(const QString &filePath)
{
    return throwError( tr("Cannot save to file \"%1\"!").arg(filePath) );
}

QJSValue Scriptable::throwImportError(const QString &filePath)
{
    return throwError( tr("Cannot import file \"%1\"!").arg(filePath) );
}

bool Scriptable::hasUncaughtException() const
{
    return m_hasUncaughtException;
}

void Scriptable::clearExceptions()
{
    m_hasUncaughtException = false;
    m_uncaughtException = QJSValue();
    m_uncaughtExceptionStack.clear();
}

void Scriptable::setUncaughtException(const QJSValue &exc)
{
    if (m_hasUncaughtException)
        return;

    m_uncaughtException = exc;
    m_hasUncaughtException = true;
    m_uncaughtExceptionStack = m_stack;
}

QJSValue Scriptable::getPlugins()
{
    // Load plugins on demand.
    if ( m_plugins.isUndefined() && m_factory ) {
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
        m_plugins = m_engine->newQObject(new ScriptablePlugins(this, m_factory));
        m_engine->globalObject().setProperty(QStringLiteral("_copyqPlugins"), m_plugins);
        m_plugins = evaluateStrict(m_engine, QStringLiteral(
            "new Proxy({}, { get: function(_, name, _) { return _copyqPlugins.load(name); } });"
        ));
#else
        m_plugins = m_engine->newObject();
        m_engine->globalObject().setProperty(QStringLiteral("_copyqPlugins"), m_plugins);
        for (const ItemLoaderPtr &loader : m_factory->loaders()) {
            const auto obj = loader->scriptableObject();
            if (!obj)
                continue;

            auto plugin = m_engine->newObject();
            m_plugins.setProperty(loader->id(), plugin);
            installObject(obj, obj->metaObject(), plugin);
            obj->setScriptable(this);
            obj->start();
        }
#endif
    }

    return m_plugins;
}

QJSValue Scriptable::call(const QString &label, QJSValue *fn, const QVariantList &arguments)
{
    QJSValueList fnArgs;
    fnArgs.reserve( arguments.size() );
    for (const auto &argument : arguments)
        fnArgs.append( toScriptValue(argument, m_engine) );

    return call(label, fn, fnArgs);
}

QJSValue Scriptable::call(const QString &label, QJSValue *fn, const QJSValueList &arguments)
{
    m_stack.prepend(label);
    COPYQ_LOG_VERBOSE( QStringLiteral("Stack push: %1").arg(m_stack.join('|')) );
    const auto v = m_safeCall.callWithInstance(*fn, arguments);
    m_stack.pop_front();
    COPYQ_LOG_VERBOSE( QStringLiteral("Stack pop: %1").arg(m_stack.join('|')) );
    return v;
}

QJSValue Scriptable::ByteArray() const
{
    const auto arg = argument(0);
    if (arg.isUndefined())
        return newByteArray(new ScriptableByteArray());

    if (arg.isNumber())
        return newByteArray(new ScriptableByteArray(arg.toInt()));

    const auto obj = arg.toQObject();
    if (obj) {
        const auto ba = qobject_cast<ScriptableByteArray*>(obj);
        if (ba)
            return newByteArray(new ScriptableByteArray(*ba));
    }

    return newByteArray(new ScriptableByteArray(makeByteArray(arg)));
}

QJSValue Scriptable::File() const
{
    const auto arg = argument(0);
    const auto path = arg.isUndefined() ? QString() : toString(arg);
    return newQObject(new ScriptableFile(path), m_filePrototype);
}

QJSValue Scriptable::TemporaryFile() const
{
    const auto arg = argument(0);
    const auto path = arg.isUndefined() ? QString() : toString(arg);
    return newQObject(new ScriptableTemporaryFile(path), m_temporaryFilePrototype);
}

QJSValue Scriptable::Dir() const
{
    const auto arg = argument(0);
    const auto path = arg.isUndefined() ? QString() : toString(arg);
    return newQObject(new ScriptableDir(path), m_dirPrototype);
}

QJSValue Scriptable::ItemSelection() const
{
    const auto arg = argument(0);
    const auto tabName = arg.isUndefined() ? QString() : toString(arg);
    auto sel = new ScriptableItemSelection(tabName);
    auto obj = newQObject(sel, m_itemSelectionPrototype);
    sel->init(obj, m_proxy, m_tabName);
    return obj;
}

QJSValue Scriptable::Settings() const
{
    const auto arg = argument(0);
    if (arg.isUndefined())
        return newQObject(new ScriptableSettings(), m_settingsPrototype);

    return newQObject(new ScriptableSettings(toString(arg)), m_settingsPrototype);
}

QJSValue Scriptable::version()
{
    m_skipArguments = 0;
    return tr("CopyQ Clipboard Manager") + " " + versionString + "\n"
            + "Qt: " QT_VERSION_STR "\n"
#ifdef WITH_NATIVE_NOTIFICATIONS
            + "KNotifications: " KNOTIFICATIONS_VERSION_STRING "\n"
#endif
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
            + "Arch: " + QSysInfo::buildAbi() + "\n"
            + "OS: " + QSysInfo::prettyProductName() + "\n"
            ;
}

QJSValue Scriptable::help()
{
    m_skipArguments = -1;

    QString helpString;

    if ( argumentCount() == 0 ) {
        helpString.append(helpHead() + "\n");

        for (const auto &hlp : commandHelp())
            helpString.append(hlp.toString());

        helpString.append("\n" + helpTail() + "\n\n" + tr("CopyQ Clipboard Manager")
            + " " + versionString + "\n");
    } else {
        for (int i = 0; i < argumentCount(); ++i) {
            const QString &cmd = toString(argument(i));
            for (const auto &helpItem : commandHelp()) {
                if ( helpItem.cmd.contains(cmd) )
                    helpString.append(helpItem.toString());
            }
        }

        if ( helpString.isEmpty() ) {
            return throwError( tr("Command not found!") );
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
        m_proxy->showBrowser( toString(argument(0)) );
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
        m_proxy->toggleCurrentMenu();
    } else {
        const auto tabName = toString(argument(0));

        int maxItemCount = -1;
        if (argumentCount() >= 2) {
            const auto value = argument(1);
            if ( !toInt(value, &maxItemCount) || maxItemCount <= 0 ) {
                return throwError("Argument maxItemCount must be positive number");
            }
        }

        int x = -1;
        int y = -1;
        if (argumentCount() >= 3) {
            const auto xValue = argument(2);
            const auto yValue = argument(3);
            if ( !toInt(xValue, &x) || !toInt(yValue, &y) ) {
                return throwError("Coordinates must be numbers");
            }
        }

        m_proxy->toggleMenu( tabName, maxItemCount, QPoint(x, y) );
    }

    return QJSValue();
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

QJSValue Scriptable::focusPrevious()
{
    m_skipArguments = 0;

    if ( !m_proxy->focusPrevious() )
        return throwError("Failed to focus previous window");

    return QJSValue();
}

QJSValue Scriptable::preview()
{
    m_skipArguments = 1;
    return m_proxy->preview( toVariant(argument(0)) );
}

QJSValue Scriptable::filter()
{
    m_skipArguments = 1;
    if (argumentCount() == 0)
        return m_proxy->filter();

    m_proxy->filter(arg(0));
    return QJSValue();
}

void Scriptable::ignore()
{
    m_skipArguments = 0;
    m_data[mimeIgnore] = QByteArray();
}

QJSValue Scriptable::clipboard()
{
    m_skipArguments = 1;
    const QString &mime = arg(0, mimeText);
    return newByteArray( getClipboardData(mime) );
}

QJSValue Scriptable::selection()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0, mimeText);
    return newByteArray( getClipboardData(mime, ClipboardMode::Selection) );
#else
    return QJSValue();
#endif
}

QJSValue Scriptable::hasClipboardFormat()
{
    m_skipArguments = 1;
    const QString &mime = arg(0);
    return hasClipboardFormat(mime, ClipboardMode::Clipboard);
}

QJSValue Scriptable::hasSelectionFormat()
{
    m_skipArguments = 1;
#ifdef HAS_MOUSE_SELECTIONS
    const QString &mime = arg(0);
    return hasClipboardFormat(mime, ClipboardMode::Selection);
#else
    return false;
#endif
}

QJSValue Scriptable::isClipboard()
{
    return isClipboardData(m_data);
}

QJSValue Scriptable::copy()
{
    m_skipArguments = -1;
    return copy(ClipboardMode::Clipboard);
}

QJSValue Scriptable::copySelection()
{
    m_skipArguments = -1;
#ifdef HAS_MOUSE_SELECTIONS
    return copy(ClipboardMode::Selection);
#else
    return QJSValue();
#endif
}

QJSValue Scriptable::paste()
{
    m_skipArguments = 0;
    if ( !m_proxy->pasteToCurrentWindow() )
        return throwError( QStringLiteral("Failed to paste clipboard") );

    const QString option = QStringLiteral("script_paste_delay_ms");
    const auto values = m_proxy->config({option}).toMap();
    const int msec = values.value(option).toInt();
    COPYQ_LOG( QStringLiteral("Delay after paste: %1ms").arg(msec) );
    if (msec > 0)
        interruptibleSleep(msec);

    return QJSValue();
}

QJSValue Scriptable::tab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    if ( name.isNull() )
        return toScriptValue( m_proxy->tabs(), m_engine );

    m_tabName = name;
    return QJSValue();
}

QJSValue Scriptable::removeTab()
{
    m_skipArguments = 1;

    const QString &name = arg(0);
    if ( const QString error = m_proxy->removeTab(name); !error.isEmpty() )
        return throwError(error);
    return QJSValue();
}

QJSValue Scriptable::renameTab()
{
    m_skipArguments = 2;
    const QString &name = arg(0);
    const QString &newName = arg(1);
    if ( const QString error = m_proxy->renameTab(newName, name); !error.isEmpty() )
        return throwError(error);
    return QJSValue();
}

QJSValue Scriptable::tabIcon()
{
    m_skipArguments = 2;

    if (argumentCount() == 1)
        return m_proxy->tabIcon(arg(0));

    if (argumentCount() < 2)
        return throwError(argumentError());

    m_proxy->setTabIcon(arg(0), arg(1));
    return QJSValue();
}

QJSValue Scriptable::unload()
{
    const auto tabs = arguments();
    const QStringList unloaded = m_proxy->unloadTabs(tabs.isEmpty() ? m_proxy->tabs() : tabs);
    return toScriptValue(unloaded, m_engine);
}

void Scriptable::forceUnload()
{
    const auto tabs = arguments();
    m_proxy->forceUnloadTabs(tabs.isEmpty() ? m_proxy->tabs() : tabs);
}

QJSValue Scriptable::length()
{
    m_skipArguments = 0;
    return m_proxy->browserLength(m_tabName);
}

QJSValue Scriptable::select()
{
    m_skipArguments = 1;
    QJSValue value = argument(0);
    int row;
    if ( !toInt(value, &row) )
        return throwError(argumentError());

    m_proxy->browserMoveToClipboard(m_tabName, row);
    return QJSValue();
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

QJSValue Scriptable::remove()
{
    auto rows = getRows();
    m_skipArguments = rows.size();

    if ( rows.empty() )
        rows.append(0);

    if ( const auto error = m_proxy->browserRemoveRows(m_tabName, rows); !error.isEmpty() )
        return throwError(error);
    return QJSValue();
}

QJSValue Scriptable::move()
{
    m_skipArguments = 1;

    int row;
    if ( !toInt(argument(0), &row) ) {
        return throwError(argumentError());
    }

    m_proxy->browserMoveSelected(row);
    return QJSValue();
}

void Scriptable::edit()
{
    m_skipArguments = -1;

    QByteArray content;
    int row = -1;
    int editRow = -1;
    bool changeClipboard = true;

    const int len = argumentCount();
    for ( int i = 0; i < len; ++i ) {
        const QJSValue value = argument(i);
        if (i > 0)
            content.append(m_inputSeparator.toUtf8());
        if ( toInt(value, &row) ) {
            editRow = (i == 0) ? row : -1;
            changeClipboard = i == 0 && row < 0;
            const QByteArray bytes = row >= 0 ? m_proxy->browserItemData(m_tabName, row, mimeText)
                                              : getClipboardData(mimeText);
            content.append(bytes);
        } else {
            content.append( toByteArray(value) );
        }
    }

    editContent(editRow, mimeText, content, changeClipboard);
}

QJSValue Scriptable::editItem()
{
    m_skipArguments = 3;

    int editRow;
    if ( !toInt(argument(0), &editRow) )
        return throwError(argumentError());

    const auto format = arg(1, mimeText);
    const bool changeClipboard = editRow < 0;

    QByteArray content;
    if ( argumentCount() > 2 )
        content = makeByteArray(argument(2));
    else if (editRow >= 0)
        content = m_proxy->browserItemData(m_tabName, editRow, format);
    else
        content = getClipboardData(format);

    editContent(editRow, format, content, changeClipboard);
    return {};
}

QJSValue Scriptable::read()
{
    m_skipArguments = -1;

    QByteArray result;
    QString mime(mimeText);
    QJSValue value;

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
            mime = toString(value);
        }
    }

    if (!used)
        result.append( getClipboardData(mime) );

    return newByteArray(result);
}

QJSValue Scriptable::write()
{
    m_skipArguments = -1;
    return changeItem(true);
}

QJSValue Scriptable::change()
{
    m_skipArguments = -1;
    return changeItem(false);
}

void Scriptable::separator()
{
    setInputSeparator( toString(argument(0)) );
    m_skipArguments = 1;
}

void Scriptable::action()
{
    QString text;
    bool anyRows = false;
    int i;
    QJSValue value;

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

    QString cmd = toString(value);

    m_skipArguments = i + 2;

    if (!anyRows) {
        text = getTextData( getClipboardData(mimeText) );
    }

    const QVariantMap data = createDataMap(mimeText, text);

    if (i < argumentCount()) {
        Command command;
        command.cmd = cmd;
        command.output = mimeText;
        command.input = mimeText;
        command.wait = false;
        command.outputTab = m_proxy->tab(m_tabName);
        command.sep = ((i + 1) < argumentCount()) ? toString( argument(i + 1) )
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
    int msec;
    if ( !toInt(argument(2), &msec) )
        msec = 8000;
    m_proxy->showMessage(title, message, QString(), msec);
}

QJSValue Scriptable::notification()
{
    m_skipArguments = -1;

    QString title;
    QString message;
    int msec = -1;
    QString icon;
    QString notificationId;
    NotificationButtonList buttons;

    for ( int i = 0; i < argumentCount(); ++i ) {
        const auto name = arg(i++);
        if ( name == QLatin1String(".title") ) {
            title = arg(i);
        } else if ( name == QLatin1String(".message") ) {
            message = arg(i);
        } else if ( name == QLatin1String(".time") ) {
            if ( !toInt(argument(i), &msec) ) {
                return throwError("Expected number after .time argument");
            }
        } else if ( name == QLatin1String(".id") ) {
            notificationId = arg(i);
        } else if ( name == QLatin1String(".icon") ) {
            icon = arg(i);
        } else if ( name == QLatin1String(".button") ) {
            NotificationButton button;
            button.name = arg(i);
            button.script = arg(++i);
            button.data = makeByteArray( argument(++i) );
            buttons.items.append(button);
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

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->saveTab(m_tabName, getAbsoluteFilePath(filePath)) )
        return throwSaveError(filePath);

    return QJSValue();
}

QJSValue Scriptable::importTab()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->loadTab(getAbsoluteFilePath(filePath)) )
        return throwImportError(filePath);

    return QJSValue();
}

QJSValue Scriptable::importData()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->importData(getAbsoluteFilePath(filePath)) )
        return throwImportError(filePath);

    return QJSValue();
}

QJSValue Scriptable::exportData()
{
    m_skipArguments = 1;

    const auto filePath = arg(0);
    if ( filePath.isNull() )
        return throwError(argumentError());

    if ( !m_proxy->exportData(getAbsoluteFilePath(filePath)) )
        return throwSaveError(filePath);

    return QJSValue();
}

QJSValue Scriptable::config()
{
    m_skipArguments = -1;

    const QVariantList nameValueInput = argumentsAsVariants();
    if (nameValueInput.isEmpty())
        return m_proxy->configDescription();

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
    if ( nameValue.size() == 1 ) {
        const auto value = nameValue.constBegin().value();
        return value.type() == QVariant::StringList || value.type() == QVariant::List
            ? toScriptValue(value, m_engine)
            : toScriptValue(value.toString(), m_engine);
    }

    QStringList output;
    for (auto it = nameValue.constBegin(); it != nameValue.constEnd(); ++it) {
        const auto name = it.key();
        const auto value = it.value();
        const auto textValue =
            value.type() == QVariant::StringList || value.type() == QVariant::List
            ? value.toStringList().join(',')
            : value.toString();
        output.append( name + "=" + textValue );
    }

    return toScriptValue(output, m_engine);
}

QJSValue Scriptable::toggleConfig()
{
    m_skipArguments = 1;
    const auto optionName = arg(0);
    if ( optionName.isEmpty() )
        return throwError(argumentError());

    const auto result = m_proxy->toggleConfig(optionName);
    if ( result.type() != QVariant::Bool ) {
        return throwError( QStringLiteral("Invalid boolean option \"%1\"!").arg(optionName) );
    }

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
    info.insert("data", itemDataPath());

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
#ifdef COPYQ_GLOBAL_SHORTCUTS
                "1"
#else
                "0"
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

#ifdef COPYQ_WITH_X11
                "/X11"
#endif
                );

    const QString name = arg(0);
    if (!name.isEmpty())
        return info.value(name);

    QString result;
    for (auto it = info.constBegin(); it != info.constEnd(); ++it)
        result.append( QStringLiteral("%1: %2\n").arg(it.key(), it.value()) );
    result.chop(1);

    return result;
}

QJSValue Scriptable::eval()
{
    const auto script = arg(0);
    const auto result = eval(script);
    if ( result.isError() )
        setUncaughtException(result);
    m_skipArguments = -1;
    return result;
}

QJSValue Scriptable::source()
{
    const auto scriptFilePath = arg(0);

    QByteArray script;
    {
        QFile scriptFile( getAbsoluteFilePath(scriptFilePath) );
        if ( !scriptFile.open(QIODevice::ReadOnly) ) {
            return throwError(
                    QStringLiteral("Failed to open \"%1\": %2")
                        .arg(scriptFilePath,
                             scriptFile.errorString()) );
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

QJSValue Scriptable::str()
{
    m_skipArguments = 1;
    return arg(0);
}

QJSValue Scriptable::input()
{
    m_skipArguments = 0;

    if ( !getByteArray(m_input) )
        m_input = readInput();

    return m_input;
}

QJSValue Scriptable::toUnicode()
{
    m_skipArguments = 2;

    const auto bytes = makeByteArray(argument(0));

    if (argumentCount() >= 2)
        return ::toUnicode(bytes, argument(1), this);

    if (argumentCount() >= 1)
        return ::toUnicode(bytes, this);

    return throwError(argumentError());
}

QJSValue Scriptable::fromUnicode()
{
    m_skipArguments = 2;

    if (argumentCount() < 2)
        return throwError(argumentError());

    const QJSValue codecName = argument(1);
    const QString text = arg(0);
    return ::fromUnicode(text, codecName, this);
}

QJSValue Scriptable::dataFormats()
{
    m_skipArguments = 0;
    return toScriptValue( m_data.keys(), m_engine );
}

QJSValue Scriptable::data()
{
    m_skipArguments = 1;
    return newByteArray( m_data.value(arg(0)).toByteArray() );
}

QJSValue Scriptable::setData()
{
    m_skipArguments = 2;

    const QString mime = arg(0);
    if ( !toItemData(argument(1), mime, &m_data) )
        return false;

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

void Scriptable::print()
{
    m_skipArguments = 1;
    print(makeByteArray(argument(0)));
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

    QString expectedWidgetName;

    const auto focusPrefix = QLatin1String("focus:");
    for (int i = 0; i < argumentCount(); ++i) {
        const QString keys = toString(argument(i));

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

            if ( m_proxy->sendKeysFailed() )
                return throwError("Failed to send key presses");

            QCoreApplication::processEvents();
            if (!canContinue())
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
#else // HAS_TESTS
QJSValue Scriptable::keys()
{
    m_skipArguments = -1;
    return QJSValue();
}

QJSValue Scriptable::testSelected()
{
    m_skipArguments = 0;
    return QJSValue();
}
#endif // HAS_TESTS

void Scriptable::serverLog()
{
    m_skipArguments = 1;
    m_proxy->serverLog(arg(0));
}

QJSValue Scriptable::logs()
{
    m_skipArguments = 0;
    return QString::fromUtf8(readLogFile(50 * 1024 * 1024));
}

void Scriptable::setCurrentTab()
{
    m_skipArguments = 1;
    const QString tabName = arg(0);
    m_proxy->setCurrentTab(tabName);
}

QJSValue Scriptable::selectItems()
{
    const auto rows = getRows();
    m_skipArguments = rows.size();
    return m_proxy->selectItems(m_tabName, rows);
}

QJSValue Scriptable::selectedTab()
{
    m_skipArguments = 0;
    return m_proxy->selectedTab();
}

QJSValue Scriptable::selectedItems()
{
    m_skipArguments = 0;
    return toScriptValue( m_proxy->selectedItems(), m_engine );
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

    return toScriptValue( m_proxy->selectedItemData(selectedIndex), m_engine );
}

QJSValue Scriptable::setSelectedItemData()
{
    int selectedIndex;
    if ( !toInt(argument(0), &selectedIndex) )
        return throwError(argumentError());

    const auto data = toDataMap( argument(1) );
    return toScriptValue( m_proxy->setSelectedItemData(selectedIndex, data), m_engine );
}

QJSValue Scriptable::selectedItemsData()
{
    return toScriptValue( m_proxy->selectedItemsData().items, m_engine );
}

void Scriptable::setSelectedItemsData()
{
    m_skipArguments = 1;
    const VariantMapList dataList{fromScriptValue<QVector<QVariantMap>>( argument(0), m_engine )};
    m_proxy->setSelectedItemsData(dataList);
}

QJSValue Scriptable::escapeHtml()
{
    m_skipArguments = 1;
    return ::escapeHtml( toString(argument(0)) );
}

QJSValue Scriptable::unpack()
{
    m_skipArguments = 1;

    QVariantMap data;

    if ( !toItemData(argument(0), mimeItems, &data) )
        return throwError(argumentError());

    return toScriptValue(data, m_engine);
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

    return toScriptValue( m_proxy->browserItemData(m_tabName, row), m_engine );
}

void Scriptable::setItem()
{
    insert(2);
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

QJSValue Scriptable::md5sum()
{
    m_skipArguments = 1;
    return checksumForArgument(this, QCryptographicHash::Md5);
}

QJSValue Scriptable::sha1sum()
{
    m_skipArguments = 1;
    return checksumForArgument(this, QCryptographicHash::Sha1);
}

QJSValue Scriptable::sha256sum()
{
    m_skipArguments = 1;
    return checksumForArgument(this, QCryptographicHash::Sha256);
}

QJSValue Scriptable::sha512sum()
{
    m_skipArguments = 1;
    return checksumForArgument(this, QCryptographicHash::Sha512);
}

QJSValue Scriptable::open()
{
    m_skipArguments = -1;
    return m_proxy->openUrls( arguments() );
}

QJSValue Scriptable::execute()
{
    m_skipArguments = -1;

    m_executeStdoutData.clear();
    m_executeStdoutLastLine.clear();
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
            args.append( toString(arg) );
    }

    Action action;
    for ( ++i ; i < argumentCount(); ++i ) {
        const auto arg = argument(i);
        if ( arg.isCallable() )
            m_executeStdoutCallback = arg;
        else
            action.setInput( action.input() + makeByteArray(arg) );
    }

    action.setCommand(args);
    action.setReadOutput(true);

    connect( &action, &Action::actionOutput,
             this, &Scriptable::onExecuteOutput );

    if ( !runAction(&action) || action.actionFailed() ) {
        return throwError( QStringLiteral("Failed to run command") );
    }

    if ( m_executeStdoutCallback.isCallable() ) {
        const auto arg = toScriptValue(m_executeStdoutLastLine, m_engine);
        call( "executeStdoutCallback", &m_executeStdoutCallback, {arg} );
    }

    QJSValue actionResult = m_engine->newObject();
    actionResult.setProperty( QStringLiteral("stdout"), newByteArray(m_executeStdoutData) );
    actionResult.setProperty( QStringLiteral("stderr"), getTextData(action.errorOutput()) );
    actionResult.setProperty( QStringLiteral("exit_code"), action.exitCode() );

    m_executeStdoutData.clear();
    m_executeStdoutLastLine.clear();
    m_executeStdoutCallback = QJSValue();

    return actionResult;
}

QJSValue Scriptable::currentWindowTitle()
{
    m_skipArguments = 0;
    if ( isGuiApplication() ) {
        PlatformWindowPtr window = platformNativeInterface()->getCurrentWindow();
        return window ? window->getTitle() : QString();
    }
    return m_proxy->currentWindowTitle();
}

QJSValue Scriptable::currentClipboardOwner()
{
    return eval("currentWindowTitle()");
}

QJSValue Scriptable::dialog()
{
    m_skipArguments = -1;

    NamedValueList values;
    values.items.reserve(argumentCount() / 2);

    for ( int i = 0; i < argumentCount(); i += 2 ) {
        const QString key = arg(i);
        const QJSValue value = argument(i + 1);
        values.items.append( NamedValue(key, toVariant(value)) );
    }

    const auto dialogId = m_proxy->inputDialog(values);
    if ( !canContinue() )
        return QJSValue();

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

    if (values.items.isEmpty())
        return QJSValue();

    if (values.items.size() == 1)
        return toScriptValue( values.items.first().value, m_engine );

    QJSValue result = m_engine->newObject();

    for (const auto &value : values.items)
        result.setProperty( value.name, toScriptValue(value.value, m_engine) );

    return result;
}

QJSValue Scriptable::menuItems()
{
    const auto text = argument(0);
    if ( text.isString() ) {
        m_skipArguments = -1;
        VariantMapList items;
        for (const auto &arg : arguments())
            items.items.append(createDataMap(mimeText, arg));
        const int i = m_proxy->menuItems(items);
        if (i == -1 || i >= items.items.size())
            return QString();
        return getTextData(items.items[i]);
    }

    m_skipArguments = 1;
    const VariantMapList items{fromScriptValue<QVector<QVariantMap>>(text, m_engine)};
    if ( items.items.isEmpty() )
        return -1;
    return m_proxy->menuItems(items);
}

QJSValue Scriptable::settings()
{
    m_skipArguments = 2;

    QSettings settings(
          QSettings::IniFormat,
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-scripts");

    if (argumentCount() == 2) {
        const QString key = arg(0);
        const QJSValue value = argument(1);
        const QVariant saveValue = toVariant(value);
        settings.setValue(key, saveValue);
        return QJSValue();
    }

    if (argumentCount() == 1) {
        const auto value = settings.value(arg(0));
        return toScriptValue(value, m_engine);
    }

    return toScriptValue(settings.allKeys(), m_engine);
}

QJSValue Scriptable::dateString()
{
    m_skipArguments = 1;
    const QDateTime dateTime = QDateTime::currentDateTime();
    return dateTime.toString(arg(0));
}

QJSValue Scriptable::commands()
{
    return toScriptValue( m_proxy->commands(), m_engine );
}

void Scriptable::setCommands()
{
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), m_engine);
    m_proxy->setCommands(commands);
}

void Scriptable::addCommands()
{
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), m_engine);
    m_proxy->addCommands(commands);
}

QJSValue Scriptable::importCommands()
{
    m_skipArguments = 1;
    const auto commands = importCommandsFromText(arg(0));
    return toScriptValue(commands, m_engine);
}

QJSValue Scriptable::exportCommands()
{
    m_skipArguments = 1;
    const auto commands = fromScriptValue<QVector<Command>>(argument(0), m_engine);

    const auto exportedCommands = ::exportCommands(commands);
    if ( exportedCommands.isEmpty() )
        return throwError("Failed to export commands");

    return exportedCommands;
}

QJSValue Scriptable::networkGet()
{
    NetworkReply *reply = networkGetHelper();
    reply->data();
    return reply->toScriptValue();
}

QJSValue Scriptable::networkPost()
{
    NetworkReply *reply = networkPostHelper();
    reply->data();
    return reply->toScriptValue();
}

QJSValue Scriptable::networkGetAsync()
{
    NetworkReply *reply = networkGetHelper();
    return reply->toScriptValue();
}

QJSValue Scriptable::networkPostAsync()
{
    NetworkReply *reply = networkPostHelper();
    return reply->toScriptValue();
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

    int msec;
    if ( !toInt(argument(0), &msec) )
        return throwError(argumentError());

    interruptibleSleep(msec);

    return QJSValue();
}

QJSValue Scriptable::afterMilliseconds()
{
    m_skipArguments = 2;

    if (argumentCount() < 2)
        return throwError(argumentError());

    int msec;
    if ( !toInt(argument(0), &msec) )
        return throwError(argumentError());

    const auto fn = argument(1);
    if ( !fn.isCallable() )
        return throwError(argumentError());

    class TimedFunctionCall final : public QObject {
    public:
        TimedFunctionCall(int msec, const QJSValue &fn, Scriptable *scriptable)
            : QObject(scriptable)
            , m_timerId( startTimer(msec, Qt::PreciseTimer) )
            , m_fn(fn)
            , m_scriptable(scriptable)
        {
        }

    protected:
        void timerEvent(QTimerEvent *event) override
        {
            QObject::timerEvent(event);
            if ( m_timerId == event->timerId() ) {
                killTimer(m_timerId);
                m_scriptable->call("afterMillisecondsCallback", &m_fn);
            }
        }

    private:
        int m_timerId;
        QJSValue m_fn;
        Scriptable *m_scriptable;
    };

    new TimedFunctionCall(msec, fn, this);
    return QJSValue();
}

QVariant Scriptable::call(const QString &method, const QVariantList &arguments)
{
    if ( hasUncaughtException() )
        return QVariant();

    m_skipArguments = 2;

    auto fn = m_engine->globalObject().property(method);
    const auto result = call(method, &fn, arguments);
    if ( result.isUndefined() || result.isNull() )
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

void Scriptable::throwException(const QString &errorMessage)
{
    throwError(errorMessage);
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
    return toScriptValue( m_proxy->screenNames(), m_engine );
}

QJSValue Scriptable::queryKeyboardModifiers()
{
    const auto modifiers = m_proxy->queryKeyboardModifiers().items;
    QStringList modifierList;
    if (modifiers.testFlag(Qt::ControlModifier))
        modifierList.append("Ctrl");
    if (modifiers.testFlag(Qt::ShiftModifier))
        modifierList.append("Shift");
    if (modifiers.testFlag(Qt::AltModifier))
        modifierList.append("Alt");
    if (modifiers.testFlag(Qt::MetaModifier))
        modifierList.append("Meta");
    return toScriptValue(modifierList, m_engine);
}

QJSValue Scriptable::pointerPosition()
{
    const QPoint pos = m_proxy->pointerPosition();
    return toScriptValue(QVector<int>{pos.x(), pos.y()}, m_engine);
}

QJSValue Scriptable::setPointerPosition()
{
    m_skipArguments = 2;
    int x = 0;
    int y = 0;
    if ( !toInt(argument(0), &x) || !toInt(argument(1), &y) )
        return throwError(argumentError());

    m_proxy->setPointerPosition(x, y);

    // Apparently, on macOS the pointer position is set only after some time.
    SleepTimer t(5000);
    while ( m_proxy->pointerPosition() != QPoint(x, y) ) {
        if ( !t.sleep() )
            return throwError("Failed to set pointer position");
    }

    return QJSValue();
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

QJSValue Scriptable::iconTag()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconTag();

    const auto tag = arg(0);
    m_proxy->setIconTag(tag);

    return QJSValue();
}

QJSValue Scriptable::iconTagColor()
{
    m_skipArguments = 1;

    if (argumentCount() == 0)
        return m_proxy->iconTagColor();

    const auto color = arg(0);
    if ( !m_proxy->setIconTagColor(color) )
        return throwError("Invalid color name");

    return QJSValue();
}

QJSValue Scriptable::loadTheme()
{
    m_skipArguments = 1;

    const QString path = getAbsoluteFilePath(arg(0));
    if ( const QString error = m_proxy->loadTheme(path); !error.isEmpty() )
        return throwError(error);

    return QJSValue();
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
    )", "onClipboardChanged()");
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
    if ( canSynchronizeSelection(ClipboardMode::Selection) ) {
        COPYQ_LOG( QStringLiteral("Synchronizing to selection: Calling provideSelection()") );
        provideSelection();
    }
}

void Scriptable::synchronizeFromSelection()
{
    if( canSynchronizeSelection(ClipboardMode::Clipboard) ) {
        COPYQ_LOG( QStringLiteral("Synchronizing to clipboard: Calling provideClipboard()") );
        provideClipboard();
    }
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

QJSValue Scriptable::hasData()
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
    )", "updateClipboardData()");
}

void Scriptable::clearClipboardData()
{
    eval(R"(
    if (isClipboard()) {
        setTitle();
        hideDataNotification();
    }
    )", "clearClipboardData()");
}

QJSValue Scriptable::runAutomaticCommands()
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

    const QString menuItemProperty = QStringLiteral("menuItem");
    const QString enabledProperty = QStringLiteral("enabled");
    bool running = false;
    bool restart = false;
    connect(&timer, &QTimer::timeout, &loop, [&]() {
        if ( bytes.isEmpty() )
            return;

        if (running) {
            restart = true;
            return;
        }
        running = true;

        do {
            restart = false;
            const int currentRun = bytes.toInt();

            getActionData(actionId);
            const QStringList matchCommands =
                m_data.value(COPYQ_MIME_PREFIX "match-commands").toStringList();

            PerformanceLogger logger( QStringLiteral("Menu item filters") );

            for (int i = 0; i < matchCommands.length(); ++i) {
                const auto obj = m_engine->newObject();
                m_engine->globalObject().setProperty(menuItemProperty, obj);
                const bool enabled = canExecuteCommandFilter(matchCommands[i]);
                QVariantMap menuItem = toDataMap(obj);
                menuItem[enabledProperty] = enabled && menuItem.value(enabledProperty, true).toBool();
                if ( restart || !m_proxy->enableMenuItem(actionId, currentRun, i, menuItem) )
                    break;
            }
        } while (restart);

        running = false;
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
        fromScriptValue<QStringList>(eval("clipboardFormatsToSave()"), m_engine) );

    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);
    connect( &monitor, &ClipboardMonitor::clipboardChanged,
             this, &Scriptable::onMonitorClipboardChanged );
    connect( &monitor, &ClipboardMonitor::clipboardUnchanged,
             this, &Scriptable::onMonitorClipboardUnchanged );
    connect( &monitor, &ClipboardMonitor::synchronizeSelection,
             this, &Scriptable::onSynchronizeSelection );
    connect( &monitor, &ClipboardMonitor::fetchCurrentClipboardOwner,
             this, &Scriptable::onFetchCurrentClipboardOwner );
    connect( &monitor, &ClipboardMonitor::saveData,
             m_proxy, [this](const QVariantMap &data) {
                 m_data = data;
                 eval("saveData()");
             } );

    monitor.startMonitoring();
    setClipboardMonitorRunning(true);
    loop.exec();
    setClipboardMonitorRunning(false);
}

void Scriptable::provideClipboard()
{
    provideClipboard(ClipboardMode::Clipboard);
}

void Scriptable::provideSelection()
{
    provideClipboard(ClipboardMode::Selection);
}

QJSValue Scriptable::isClipboardMonitorRunning()
{
    return ::isClipboardMonitorRunning();
}

QJSValue Scriptable::clipboardFormatsToSave()
{
    if (!m_factory)
        return toScriptValue(QStringList(), m_engine);

    QStringList formats = m_factory->formatsToSave();
    COPYQ_LOG( "Clipboard formats to save: " + formats.join(", ") );

    for (const auto &command : m_proxy->automaticCommands()) {
        if ( !command.input.isEmpty() && !formats.contains(command.input) ) {
            COPYQ_LOG( QStringLiteral("Clipboard format to save for command \"%1\": %2")
                       .arg(command.name, command.input) );
            formats.append(command.input);
        }
    }

    return toScriptValue(formats, m_engine);
}

QJSValue Scriptable::styles()
{
    return toScriptValue( m_proxy->styles(), m_engine );
}

void Scriptable::collectScriptOverrides()
{
    m_skipArguments = 1;
    auto globalObject = engine()->globalObject();

    QVector<int> overrides;
    if (isOverridden(globalObject, QStringLiteral("paste")))
        overrides.append(ScriptOverrides::Paste);
    if (isOverridden(globalObject, QStringLiteral("onItemsAdded")))
        overrides.append(ScriptOverrides::OnItemsAdded);
    if (isOverridden(globalObject, QStringLiteral("onItemsRemoved")))
        overrides.append(ScriptOverrides::OnItemsRemoved);
    if (isOverridden(globalObject, QStringLiteral("onItemsChanged")))
        overrides.append(ScriptOverrides::OnItemsChanged);
    if (isOverridden(globalObject, QStringLiteral("onTabSelected")))
        overrides.append(ScriptOverrides::OnTabSelected);
    if (isOverridden(globalObject, QStringLiteral("onItemsLoaded")))
        overrides.append(ScriptOverrides::OnItemsLoaded);

    m_proxy->setScriptOverrides(overrides);
}

void Scriptable::onExecuteOutput(const QByteArray &output)
{
    m_executeStdoutData.append(output);

    if ( m_executeStdoutCallback.isCallable() ) {
        m_executeStdoutLastLine.append( getTextData(output) );
        auto lines = m_executeStdoutLastLine.split('\n');
        m_executeStdoutLastLine = lines.takeLast();
        if ( !lines.isEmpty() ) {
            const auto arg = toScriptValue(lines, m_engine);
            call( "executeStdoutCallback", &m_executeStdoutCallback, {arg} );
        }
    }
}

void Scriptable::onMonitorClipboardChanged(const QVariantMap &data, ClipboardOwnership ownership)
{
    COPYQ_LOG( QStringLiteral("onMonitorClipboardChanged: %1 %2, owner is \"%3\"")
               .arg(
                   QString::fromLatin1(
                       ownership == ClipboardOwnership::Own ? "own"
                       : ownership == ClipboardOwnership::Foreign ? "foreign"
                       : "hidden"),
                   QString::fromLatin1(isClipboardData(data) ? "clipboard" : "selection"),
                   getTextData(data, mimeOwner)
               ) );

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

void Scriptable::onSynchronizeSelection(ClipboardMode sourceMode, uint sourceTextHash, uint targetTextHash)
{
#ifdef HAS_MOUSE_SELECTIONS
    QVariantMap data;
    data[COPYQ_MIME_PREFIX "source-text-hash"] = QByteArray::number(sourceTextHash);
    data[COPYQ_MIME_PREFIX "target-text-hash"] = QByteArray::number(targetTextHash);
    const auto command = sourceMode == ClipboardMode::Clipboard
        ? "copyq --clipboard-access synchronizeToSelection"
        : "copyq --clipboard-access synchronizeFromSelection";
    m_proxy->runInternalAction(data, command);
#else
    Q_UNUSED(sourceMode)
    Q_UNUSED(sourceTextHash)
    Q_UNUSED(targetTextHash)
#endif
}

void Scriptable::onFetchCurrentClipboardOwner(QString *owner)
{
    const QJSValue result = eval("currentClipboardOwner()");
    if ( !result.isError() )
        *owner = toString(result);
}

bool Scriptable::sourceScriptCommands()
{
    const auto commands = m_proxy->scriptCommands();
    for (const auto &command : commands) {
        const auto script = QStringLiteral("(function(){%1\n;})()").arg(command.cmd);
        const auto label = QStringLiteral("source@<%1>").arg(command.name);
        eval(script, label);
        if ( hasUncaughtException() ) {
            processUncaughtException(label);
            return false;
        }
    }

    return true;
}

int Scriptable::executeArguments(const QStringList &args)
{
    if ( !sourceScriptCommands() )
        return CommandError;

    const int exitCode = executeArgumentsSimple(args);

    if (exitCode == CommandFinished)
        setActionData();

    // Destroy objects so destructors are run before script finishes
    // (e.g. file writes are flushed or temporary files are automatically removed).
    m_engine->collectGarbage();

    return exitCode;
}

int Scriptable::executeArgumentsSimple(const QStringList &args)
{
    /* Special arguments:
     * "-"  read this argument from stdin
     * "--" read all following arguments without control sequences
     */
    QJSValueList fnArgs;
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
    QJSValue result;

    int skipArguments = 0;
    if (!fnArgs.isEmpty() && toString(fnArgs[0]) == "--clipboard-access")
        ++skipArguments;

    auto globalObject = engine()->globalObject();
    const auto evalFn = globalObject.property("eval");

    QString label;
    while ( skipArguments < fnArgs.size() && canContinue() && !hasUncaughtException() ) {
        if ( result.isCallable() ) {
            const auto arguments = fnArgs.mid(skipArguments);
            if ( result.strictlyEquals(evalFn) )
                globalObject.setProperty( QStringLiteral("arguments"), toScriptValue(arguments, m_engine) );
            m_skipArguments = -1;
            result = call( label, &result, arguments );
            if (m_skipArguments == -1)
                break;
            skipArguments += m_skipArguments;
        } else {
            cmd = toString(fnArgs[skipArguments]);
            label = scriptToLabel(cmd);
            result = eval(cmd, label);
            ++skipArguments;
        }
    }

    if ( result.isCallable() && canContinue() && !hasUncaughtException() ) {
        const QString label2 = QStringLiteral("eval(arguments[%1])()").arg(skipArguments - 1);
        result = call( label2, &result, fnArgs.mid(skipArguments) );
    }

    int exitCode;

    if (m_failed) {
        exitCode = CommandError;
    } else if (m_abort != Abort::None) {
        exitCode = CommandFinished;
    } else if ( hasUncaughtException() ) {
        processUncaughtException(QString());
        exitCode = CommandException;
    } else {
        const auto message = serializeScriptValue(result, this);
        print(message);
        exitCode = CommandFinished;
    }

    return exitCode;
}

void Scriptable::processUncaughtException(const QString &cmd)
{
    if ( !hasUncaughtException() )
        return;

    const auto exceptionName = m_uncaughtException.toString()
            .remove(QRegularExpression("^Error: "))
            .trimmed();

    const QString backtraceText = exceptionBacktrace(m_uncaughtException, m_uncaughtExceptionStack);
    const auto exceptionText = QStringLiteral("ScriptError: %3%4").arg(exceptionName, backtraceText);

    // Show exception popups only if the script was launched from application.
    // (avoid it if launched from command line).
    if (m_actionId != -1) {
        showExceptionMessage(exceptionName);

        auto label = m_actionName;
        if ( !cmd.isEmpty() )
            label.append("::" + cmd);

        if (label.isEmpty())
            label = QStringLiteral("Exception in command: ");
        else
            label = QStringLiteral("Exception in command \"%1\": ").arg(label);
        m_proxy->serverLog(label + exceptionText);
    }

    printError(exceptionText.toUtf8());
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
    m_proxy->showMessage(title, message, QString(QChar(IconCircleExclamation)), 8000, notificationId);
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

QVector<QVariantMap> Scriptable::getItemArguments(int begin, int end, QString *error)
{
    if (begin == end) {
        *error = QStringLiteral("Expected item arguments");
        return {};
    }

    const auto firstArg = argument(begin);
    if (firstArg.isArray()) {
        if (end - begin != 1) {
            *error = QStringLiteral("Unexpected multiple item list arguments");
            return {};
        }
        return getItemList(0, firstArg.property("length").toInt(), firstArg);
    }

    QVector<QVariantMap> items;
    if (firstArg.toVariant().canConvert<QVariantMap>()) {
        for (int i = begin; i < end; ++i)
            items.append( fromScriptValue<QVariantMap>(argument(i), m_engine) );
    } else if (end - begin == 1) {
        QVariantMap data;
        QJSValue value = argument(begin);
        setTextData( &data, toString(value) );
        items.append(data);
    } else {
        if ((end - begin) % 2 != 0) {
            *error = QStringLiteral("Unexpected uneven number of mimeType/data arguments");
            return {};
        }
        QVariantMap data;
        for (int i = begin; i < end; i += 2) {
            // MIME
            const QString mime = toString(argument(i));
            // DATA
            toItemData( argument(i + 1), mime, &data );
        }
        items.append(data);
    }

    return items;
}

QVector<QVariantMap> Scriptable::getItemList(int begin, int end, const QJSValue &arguments)
{
    if (end < begin)
        return {};

    QVector<QVariantMap> items;
    items.reserve(end - begin);

    for (int i = begin; i < end; ++i) {
        const auto arg = arguments.property( static_cast<quint32>(i) );
        if ( arg.isObject() && getByteArray(arg) == nullptr && !arg.isArray() )
            items.append( fromScriptValue<QVariantMap>(arg, m_engine) );
        else
            items.append( createDataMap(mimeText, toString(arg)) );
    }

    return items;
}

QJSValue Scriptable::copy(ClipboardMode mode)
{
    const int args = argumentCount();

    if (args == 0) {
        // Reset clipboard first.
        const QString mime = COPYQ_MIME_PREFIX "invalid";
        const QByteArray value = "invalid";
        const QVariantMap data = createDataMap(mime, value);
        m_proxy->setClipboard(data, mode);

        m_proxy->copyFromCurrentWindow();

        // Wait for clipboard to be set.
        for (int i = 0; i < 10; ++i) {
            if ( getClipboardData(mime) != value )
                return true;
            waitFor(5 + i * 25);
        }

        return throwError( tr("Failed to copy to clipboard!") );
    }

    QString error;
    const QVector<QVariantMap> items = getItemArguments(0, args, &error);
    if ( !error.isEmpty() )
        return throwError(error);

    if (items.size() != 1)
        return throwError(QStringLiteral("Expected single item"));

    m_proxy->setClipboard(items[0], mode);
    return true;
}

void Scriptable::abortEvaluation(Abort abort)
{
    m_abort = abort;
    throwError("Evaluation aborted");

    if (m_abort == Abort::AllEvaluations)
        m_proxy->clientDisconnected();
    else
        m_proxy->abortEvaluation();

    emit finished();
}

QJSValue Scriptable::changeItem(bool create)
{
    int row;
    int args = argumentCount();
    int i;

    // [ROW]
    if ( toInt(argument(0), &row) ) {
        i = 1;
    } else {
        row = 0;
        i = 0;
    }

    QString error;
    const VariantMapList items{getItemArguments(i, args, &error)};
    if ( !error.isEmpty() )
        return throwError(error);

    if (create) {
        error = m_proxy->browserInsert(m_tabName, row, items);
        if ( !error.isEmpty() )
            return throwError(error);
    } else {
        error = m_proxy->browserChange(m_tabName, row, items);
        if ( !error.isEmpty() )
            return throwError(error);
    }
    return QJSValue();
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

void Scriptable::editContent(
        int editRow, const QString &format, const QByteArray &content, bool changeClipboard)
{
    if ( m_proxy->browserOpenEditor(m_tabName, editRow, format, content, changeClipboard) )
        return;

    m_proxy->showBrowser(m_tabName);
    if (editRow >= 0) {
        m_proxy->browserSetCurrent(m_tabName, editRow);
        m_proxy->browserEditRow(m_tabName, editRow, format);
    } else {
        m_proxy->browserEditNew(m_tabName, format, content, changeClipboard);
    }
}

QJSValue Scriptable::screenshot(bool select)
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
        return QJSValue();
    }

    return newByteArray(imageData);
}

QJSValue Scriptable::eval(const QString &script, const QString &label)
{
    m_stack.prepend(QStringLiteral("eval:") + label);
    COPYQ_LOG_VERBOSE( QStringLiteral("Stack push: %1").arg(m_stack.join('|')) );
    const auto result = m_safeEval.call({QJSValue(script)});
    m_stack.pop_front();
    COPYQ_LOG_VERBOSE( QStringLiteral("Stack pop: %1").arg(m_stack.join('|')) );

    if (m_abort != Abort::None) {
        clearExceptions();
        if (m_abort == Abort::AllEvaluations)
            abortEvaluation(Abort::AllEvaluations);
        else
            m_abort = Abort::None;

        return QJSValue();
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

QJSValue Scriptable::eval(const QString &script)
{
    return eval(script, scriptToLabel(script));
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

        const auto exitCode = executeArgumentsSimple(cmd1.mid(1));
        action->setExitCode(exitCode);
        m_failed = false;
        clearExceptions();
        if (m_abort == Abort::AllEvaluations)
            abortEvaluation(Abort::AllEvaluations);
        else
            m_abort = Abort::None;

        m_action = oldAction;
        m_input = oldInput;

        return true;
    }

    // Update data for the new action.
    setActionData();

    action->setWorkingDirectory( getCurrentPath() );

    QEventLoop loop;
    connect(action, &Action::actionFinished, &loop, &QEventLoop::quit);
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);
    action->start();
    loop.exec();

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
        PerformanceLogger logger( QStringLiteral("Command \"%1\"").arg(command.name) );

        if ( command.outputTab.isEmpty() )
            command.outputTab = tabName;

        QStringList arguments;
        if ( !canExecuteCommand(command, &arguments) )
            continue;

        if ( canContinue() && !command.cmd.isEmpty() ) {
            Action action;
            action.setCommand(command.cmd, arguments);
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

bool Scriptable::canExecuteCommand(const Command &command, QStringList *arguments)
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

    const QString text = getTextData(m_data);
    arguments->append(text);

    // Verify that and text matches given regexp.
    if ( !command.re.pattern().isEmpty() ) {
        QRegularExpressionMatchIterator it = command.re.globalMatch(text);
        if ( !it.isValid() || !it.hasNext() )
            return false;

        while (it.hasNext())
            arguments->append( it.next().capturedTexts().mid(1) );
    }

    // Verify that window title matches given regexp.
    if ( !command.wndre.pattern().isEmpty() && !getTextData(m_data, mimeWindowTitle).contains(command.wndre) )
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

bool Scriptable::verifyClipboardAccess()
{
    if ( isGuiApplication() )
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

    const auto type = mode == ClipboardMode::Clipboard ? "clipboard" : "selection";
    ClipboardSpy spy(mode, owner);
    connect( this, &Scriptable::finished, &spy, &ClipboardSpy::stop );

    if ( !spy.setClipboardData(m_data) ) {
        if (m_abort != Abort::None)
            return;

        log( QStringLiteral("Failed to provide %1").arg(type), LogWarning );
        return;
    }

    if (m_abort != Abort::None)
        return;

    COPYQ_LOG( QStringLiteral("Started providing %1").arg(type) );
    spy.wait(-1, 8000);
    COPYQ_LOG( QStringLiteral("Finished providing %1").arg(type) );
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

    const VariantMapList items{getItemList(argumentsBegin, argumentsEnd, argumentsArray())};
    if ( const auto error = m_proxy->browserInsert(m_tabName, row, items); !error.isEmpty() )
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

QVariantList Scriptable::argumentsAsVariants()
{
    QVariantList args;
    args.reserve( argumentCount() );

    for (int i = 0; i < argumentCount(); ++i)
        args.append( toVariant(argument(i)) );

    return args;
}

void Scriptable::print(const QByteArray &message)
{
    if (m_action) {
        m_action->appendOutput(message);
    } else if ( canUseStandardOutput() ) {
        QFile f;
        f.open(stdout, QIODevice::WriteOnly);
        f.write(message);
    }
}

void Scriptable::printError(const QByteArray &message)
{
    if (m_action) {
        m_action->appendErrorOutput(message);
    } else if ( canUseStandardOutput() ) {
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
    return m_proxy->getClipboardData(mime, mode);
}

bool Scriptable::hasClipboardFormat(const QString &mime, ClipboardMode mode)
{
    return m_proxy->hasClipboardFormat(mime, mode);
}

bool Scriptable::canSynchronizeSelection(ClipboardMode targetMode)
{
#ifdef HAS_MOUSE_SELECTIONS
    if (!verifyClipboardAccess())
        return false;

    // Make sure a clipboard instance is created so as to monitor any changes;
    clipboardInstance();

    SleepTimer tMin(50);

    // Avoid changing clipboard after a text is selected just before it's copied
    // with a keyboard shortcut.
    SleepTimer t(5000);
    while ( QGuiApplication::queryKeyboardModifiers() != Qt::NoModifier ) {
        if ( !t.sleep() && !canContinue() ) {
            COPYQ_LOG("Sync: Cancelled - keyboard modifiers still being held");
            return false;
        }
    }

    const auto sourceMode = targetMode == ClipboardMode::Selection ? ClipboardMode::Clipboard : ClipboardMode::Selection;

    // Wait at least few milliseconds before synchronization
    // to avoid overriding just changed clipboard/selection.
    while ( tMin.sleep() ) {}

    // Stop if the clipboard/selection text already changed again.
    const QVariantMap sourceData = clipboardInstance()->data(
        sourceMode, {mimeTextUtf8, mimeText, mimeUriList});
    QByteArray source;
    if (!sourceData.isEmpty()) {
        source = sourceData.value(mimeText).toByteArray();
        const QString owner = sourceData.value(mimeOwner).toString();
        if ( owner.isEmpty() && !source.isEmpty() ) {
            const auto sourceTextHash = m_data.value(COPYQ_MIME_PREFIX "source-text-hash").toByteArray().toUInt();
            const uint newSourceTextHash = qHash(source);
            if (sourceTextHash != newSourceTextHash) {
                COPYQ_LOG("Sync: Cancelled - source text changed");
                return false;
            }
        }
    } else {
        COPYQ_LOG("Sync: Failed to fetch source data");
    }

    const QVariantMap targetData = clipboardInstance()->data(
        targetMode, {mimeText});
    if (!targetData.isEmpty()) {
        const QByteArray target = targetData.value(mimeText).toByteArray();
        const QString owner = targetData.value(mimeOwner).toString();
        if ( owner.isEmpty() && !target.isEmpty() ) {
            const auto targetTextHash = m_data.value(COPYQ_MIME_PREFIX "target-text-hash").toByteArray().toUInt();
            const uint newTargetTextHash = qHash(target);
            if (targetTextHash != newTargetTextHash) {
                COPYQ_LOG("Sync: Cancelled - target text changed");
                return false;
            }
        }

        // Stop if the clipboard and selection text is already synchronized
        // or user selected text and copied it to clipboard.
        if (!sourceData.isEmpty() && source == target) {
            COPYQ_LOG("Sync: Cancelled - target text is already same as source");
            return false;
        }
    } else {
        COPYQ_LOG("Sync: Failed to fetch target data");
    }

    if (m_abort != Abort::None) {
        COPYQ_LOG("Sync: Aborting");
        return false;
    }

    m_data = sourceData;
    return true;
#else
    Q_UNUSED(targetMode)
    return false;
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

QJSValue Scriptable::readInput()
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
        return QJSValue();
    }

    inputReaderThread->deleteLater();
    COPYQ_LOG(QStringLiteral("Read Input %1 bytes").arg(inputReaderThread->input.length()));
    return newByteArray(inputReaderThread->input);
}

PlatformClipboard *Scriptable::clipboardInstance()
{
    if (m_clipboard == nullptr)
        m_clipboard = platformNativeInterface()->clipboard();
    return m_clipboard.get();
}

void Scriptable::interruptibleSleep(int msec)
{
    if ( !canContinue() )
        return;

    QEventLoop loop;
    connect(this, &Scriptable::finished, &loop, &QEventLoop::quit);

    QTimer t;
    t.setTimerType(Qt::PreciseTimer);
    t.setInterval(msec);
    connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start();

    loop.exec();
}

NetworkReply *Scriptable::networkGetHelper()
{
    m_skipArguments = 1;
    const QString url = arg(0);
    return NetworkReply::get(url, this);
}

NetworkReply *Scriptable::networkPostHelper()
{
    m_skipArguments = 2;
    const QString url = arg(0);
    const QByteArray postData = makeByteArray(argument(1));
    return NetworkReply::post(url, postData, this);
}

QJSValue Scriptable::newQObject(QObject *obj, const QJSValue &prototype) const
{
    auto value = m_engine->newQObject(obj);
    value.setPrototype(prototype);
    return value;
}

void Scriptable::installObject(QObject *fromObj, const QMetaObject *metaObject, QJSValue &toObject)
{
    const auto from = m_engine->newQObject(fromObj);

    for (int i = 0; i < metaObject->methodCount(); ++i) {
        const QMetaMethod slot = metaObject->method(i);
        if ( slot.methodType() != QMetaMethod::Slot )
            continue;

        if ( slot.access() != QMetaMethod::Public )
            continue;

        if ( slot.parameterCount() != 0 )
            continue;

        const QLatin1String name( slot.name() );
        const bool hasByteArrayReturnType = slot.returnType() == QMetaType::QByteArray;

        // Allow passing variable number of arguments to scriptable methods
        // and handle exceptions.
        const auto v = hasByteArrayReturnType
            ? m_createFnB.call({from, name})
            : m_createFn.call({from, name});
        if ( v.isError() ) {
            log( QStringLiteral("Exception while wrapping %1.%2: %3").arg(fromObj->objectName(), name, v.toString()), LogError );
            Q_ASSERT(false);
        } else {
            toObject.setProperty(name, v);
        }
    }

    for (int i = 0; i < metaObject->propertyCount(); ++i) {
        const QMetaProperty prop = metaObject->property(i);
        const QLatin1String name( prop.name() );
        const auto v = m_createProperty.call({toObject, name, from});
        if ( v.isError() ) {
            log( QStringLiteral("Exception while adding property %1.%2: %3").arg(fromObj->objectName(), name, v.toString()), LogError );
            Q_ASSERT(false);
        }
    }
}

NetworkReply *NetworkReply::get(const QString &url, Scriptable *scriptable)
{
    return new NetworkReply(url, QByteArray(), scriptable);
}

NetworkReply *NetworkReply::post(const QString &url, const QByteArray &postData, Scriptable *scriptable)
{
    return new NetworkReply(url, postData, scriptable);
}

NetworkReply::~NetworkReply()
{
    if (m_reply)
        m_reply->deleteLater();
}

QJSValue NetworkReply::data()
{
    if ( !m_data.isUndefined() )
        return m_data;

    if ( !m_reply->isFinished() && m_scriptable->canContinue() ) {
        QEventLoop loop;
        connect(m_scriptable, &Scriptable::finished, &loop, &QEventLoop::quit);
        connect(m_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    if ( !m_reply->isFinished() )
        return QJSValue();

    m_data = m_scriptable->newByteArray(m_rawData);

    return m_data;
}

QJSValue NetworkReply::error()
{
    data();

    if (m_reply->error() != QNetworkReply::NoError)
        return m_reply->errorString();

    return QJSValue();
}

QJSValue NetworkReply::status()
{
    data();

    const QVariant v = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (v.isValid())
        return v.toInt();
    return QJSValue();
}

QJSValue NetworkReply::redirect()
{
    data();

    const QVariant v = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (v.isValid())
        return v.toUrl().resolved(m_reply->url()).toString();
    return QJSValue();
}

QJSValue NetworkReply::headers()
{
    data();

    QJSValue headers = m_scriptable->engine()->newArray();
    int i = 0;
    for ( const auto &header : m_reply->rawHeaderList() ) {
        QJSValue pair = m_scriptable->engine()->newArray();
        pair.setProperty( 0, m_scriptable->newByteArray(header) );
        pair.setProperty( 1, m_scriptable->newByteArray(m_reply->rawHeader(header)) );
        headers.setProperty( static_cast<quint32>(i), pair );
        ++i;
    }

    return headers;
}

QJSValue NetworkReply::finished()
{
    return m_reply->isFinished();
}

NetworkReply::NetworkReply(const QString &url, const QByteArray &postData, Scriptable *scriptable)
    : QObject(scriptable)
    , m_scriptable(scriptable)
    , m_manager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
{
    if (postData.isEmpty())
        m_reply = m_manager->get(QNetworkRequest(url));
    else
        m_reply = m_manager->post(QNetworkRequest(url), postData);

    connect(m_reply, &QNetworkReply::readyRead, this, [this](){
        const qint64 available = m_reply->bytesAvailable();
        m_rawData.append( m_reply->read(available) );
    });
    const qint64 available = m_reply->bytesAvailable();
    m_rawData.append( m_reply->read(available) );
}

QJSValue NetworkReply::toScriptValue()
{
    if ( m_self.isUndefined() )
        m_self = m_scriptable->engine()->newQObject(this);
    return m_self;
}

ScriptablePlugins::ScriptablePlugins(Scriptable *scriptable, ItemFactory *factory)
    : QObject(scriptable)
    , m_scriptable(scriptable)
    , m_factory(factory)
{
}

QJSValue ScriptablePlugins::load(const QString &name)
{
    const auto it = m_plugins.find(name);
    if (it != std::end(m_plugins))
        return it.value();

    auto obj = m_factory->scriptableObject(name);
    if (!obj) {
        m_scriptable->throwError(
            QStringLiteral("Plugin \"%1\" is not installed").arg(name) );
        return QJSValue();
    }

    auto plugin = m_scriptable->engine()->newObject();
    m_plugins.insert(name, plugin);
    m_scriptable->installObject(obj, obj->metaObject(), plugin);
    obj->setScriptable(m_scriptable);
    obj->start();

    return plugin;
}

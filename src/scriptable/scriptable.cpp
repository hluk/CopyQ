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
#include "common/common.h"
#include "common/log.h"
#include "common/sleeptimer.h"
#include "common/version.h"
#include "common/textdata.h"
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
#include <QRegExp>
#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSettings>
#include <QUrl>
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

template <typename T>
struct ScriptValueFactory {
    static QScriptValue create(const T &value, Scriptable *) { return QScriptValue(value); }
};

template <typename T>
struct ScriptValueFactory< QList<T> > {
    static QScriptValue create(const QList<T> &list, Scriptable *scriptable)
    {
        QScriptValue array = scriptable->engine()->newArray();
        for ( int i = 0; i < list.size(); ++i ) {
            const auto value = ScriptValueFactory<T>::create(list[i], scriptable);
            array.setProperty( static_cast<quint32>(i), value );
        }
        return array;
    }
};

template <>
struct ScriptValueFactory<QVariantMap> {
    static QScriptValue create(const QVariantMap &dataMap, Scriptable *scriptable)
    {
        QScriptValue value = scriptable->engine()->newObject();

        for ( const auto &format : dataMap.keys() )
            value.setProperty(format, scriptable->newByteArray(dataMap[format].toByteArray()));

        return value;
    }
};

template <>
struct ScriptValueFactory<QStringList> {
    static QScriptValue create(const QStringList &list, Scriptable *scriptable)
    {
        return ScriptValueFactory< QList<QString> >::create(list, scriptable);
    }
};

template <typename T>
QScriptValue toScriptValue(const T &value, Scriptable *scriptable)
{
    return ScriptValueFactory<T>::create(value, scriptable);
}

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

Scriptable::Scriptable(ScriptableProxy *proxy,
        QObject *parent)
    : QObject(parent)
    , QScriptable()
    , m_proxy(proxy)
    , m_engine(nullptr)
    , m_baClass(nullptr)
    , m_dirClass(nullptr)
    , m_fileClass(nullptr)
    , m_temporaryFileClass(nullptr)
    , m_inputSeparator("\n")
    , m_input()
    , m_connected(true)
{
}

void Scriptable::initEngine(QScriptEngine *eng)
{
    m_engine = eng;
    QScriptEngine::QObjectWrapOptions opts =
              QScriptEngine::ExcludeChildObjects
            | QScriptEngine::SkipMethodsInEnumeration
            | QScriptEngine::ExcludeSuperClassMethods
            | QScriptEngine::ExcludeSuperClassProperties
            | QScriptEngine::ExcludeSuperClassContents
            | QScriptEngine::ExcludeDeleteLater;

    QScriptValue obj = eng->newQObject(this, QScriptEngine::QtOwnership, opts);

    // Keep internal functions as parseInt() or encodeURIComponent().
    QScriptValue oldObj = eng->globalObject();
    QScriptValueIterator it(oldObj);
    while (it.hasNext()) {
        it.next();
        obj.setProperty(it.name(), it.value(), it.flags());
    }

    eng->setGlobalObject(obj);
    eng->setProcessEventsInterval(1000);

    m_baClass = new ByteArrayClass(eng);
    addScriptableClass(&obj, m_baClass);

    m_fileClass = new FileClass(eng);
    addScriptableClass(&obj, m_fileClass);

    m_temporaryFileClass = new TemporaryFileClass(eng);
    addScriptableClass(&obj, m_temporaryFileClass);

    m_dirClass = new DirClass(eng);
    addScriptableClass(&obj, m_dirClass);
}

QScriptValue Scriptable::newByteArray(const QByteArray &bytes)
{
    return m_baClass->newInstance(bytes);
}

QScriptValue Scriptable::newVariant(const QVariant &value)
{
    return value.isValid() ? engine()->newVariant(value) : QScriptValue();
}

QByteArray Scriptable::fromString(const QString &value) const
{
  QByteArray bytes = value.toUtf8();
#ifdef COPYQ_OS_WIN
  bytes.replace('\n', "\r\n");
#endif
  return bytes;
}

QString Scriptable::toString(const QScriptValue &value) const
{
    QByteArray *bytes = getByteArray(value);
    return (bytes == nullptr) ? value.toString()
                              : getTextData(*bytes);
}

QVariant Scriptable::toVariant(const QScriptValue &value) const
{
    auto bytes = getByteArray(value);
    if (bytes)
        return QVariant(*bytes);

    auto file = getFile(value);
    if (file)
        return QVariant::fromValue(file);

    return value.toVariant();
}

bool Scriptable::toInt(const QScriptValue &value, int *number) const
{
    bool ok;
    *number = toString(value).toInt(&ok);
    return ok;
}

QVariantMap Scriptable::toDataMap(const QScriptValue &value) const
{
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

QByteArray *Scriptable::getByteArray(const QScriptValue &value) const
{
    if (value.scriptClass() == m_baClass)
        return qscriptvalue_cast<QByteArray*>(value.data());
    return nullptr;
}

QByteArray Scriptable::makeByteArray(const QScriptValue &value) const
{
    QByteArray *data = getByteArray(value);
    return data ? *data : fromString(value.toString());
}

QFile *Scriptable::getFile(const QScriptValue &value) const
{
    if (value.scriptClass() == m_fileClass || value.scriptClass() == m_temporaryFileClass)
        return qscriptvalue_cast<QFile*>(value.data());
    return nullptr;
}

bool Scriptable::toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const
{
    if (mime == mimeItems) {
        const QByteArray *itemData = getByteArray(value);
        if (!itemData)
            return false;

        return deserializeData(data, *itemData);
    }

    if (value.isUndefined())
        data->insert( mime, QVariant() );
    else if (!mime.startsWith("text/") && value.scriptClass() == m_baClass)
        data->insert( mime, *getByteArray(value) );
    else
        data->insert( mime, toString(value).toUtf8() );

    return true;
}

const QString &Scriptable::getInputSeparator() const
{
    return m_inputSeparator;
}

void Scriptable::setInputSeparator(const QString &separator)
{
    m_inputSeparator = separator;
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
    return i < argumentCount() ? toString(argument(i)) : defaultValue;
}

void Scriptable::throwError(const QString &errorMessage)
{
    auto currentContext = context();
    if (!currentContext)
        currentContext = engine()->pushContext();
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
            const QString &cmd = toString(argument(i));
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
        m_proxy->showBrowser(toString(argument(0)));
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
    m_skipArguments = 2;

    if (argumentCount() == 0) {
        m_proxy->toggleMenu();
    } else {
        const auto tabName = toString(argument(0));

        int maxItemCount = -1;
        if (argumentCount() == 2) {
            const auto value = argument(1);
            if ( !toInt(value, &maxItemCount) || maxItemCount <= 0 ) {
                throwError(argumentError());
                return;
            }
        }

        m_proxy->toggleMenu(tabName, maxItemCount);
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
        texts.append( toString(argument(i)) );

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
    setTextData( &data, toString(value) );
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
            text.append( getInputSeparator() );
        if ( toInt(value, &row) ) {
            const QByteArray bytes = row >= 0 ? m_proxy->browserItemData(row, mimeText)
                                              : m_proxy->getClipboardData(mimeText);
            text.append( getTextData(bytes) );
        } else {
            text.append( toString(value) );
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
    QString sep = getInputSeparator();

    bool used = false;
    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, &row) ) {
            if (used)
                result.append(sep.toUtf8());
            used = true;
            result.append( row >= 0 ? m_proxy->browserItemData(row, mime)
                                    : m_proxy->getClipboardData(mime) );
        } else {
            mime = toString(value);
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
    setInputSeparator( toString(argument(0)) );
    m_skipArguments = 1;
}

void Scriptable::action()
{
    QString text;
    bool anyRows = false;
    int i;
    QScriptValue value;
    QString sep = getInputSeparator();

    for ( i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if (!toInt(value, &row))
            break;
        if (anyRows)
            text.append(sep);
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
        command.cmd = toString(value);
        command.output = mimeText;
        command.input = mimeText;
        command.wait = false;
        command.outputTab = m_proxy->tab();
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

    QString title = arg(0);
    QString message = arg(1);
    int msec;
    if ( !toInt(argument(2), &msec) )
        msec = 8000;
    m_proxy->showMessage(title, message, QSystemTrayIcon::Information, msec);
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
    const auto result = eval(script, "eval");
    m_skipArguments = -1;
    return result;    
}

QScriptValue Scriptable::source()
{
    const auto scriptFilePath = arg(0);

    QByteArray script;
    {
        QFile scriptFile(scriptFilePath);
        if ( !scriptFile.open(QIODevice::ReadOnly) ) {
            throwError( QString("Failed to open \"%1\": %2")
                        .arg(scriptFilePath)
                        .arg(scriptFile.errorString()) );
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
    return toString(value);
}

QScriptValue Scriptable::input()
{
    m_skipArguments = 0;

    if ( !getByteArray(m_input) ) {
        sendMessageToClient(QByteArray(), CommandReadInput);
        while ( m_connected && !getByteArray(m_input) )
            QApplication::processEvents();
    }

    return m_input;
}

QScriptValue Scriptable::dataFormats()
{
    m_skipArguments = 0;
    return toScriptValue( m_data.keys(), this );
}

QScriptValue Scriptable::data(const QScriptValue &value)
{
    m_skipArguments = 1;
    return newByteArray( m_data.value(toString(value)).toByteArray() );
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
        const QString keys = toString(argument(i));

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

QScriptValue Scriptable::index()
{
    m_skipArguments = 0;
    return currentItem();
}

QScriptValue Scriptable::escapeHtml()
{
    m_skipArguments = 1;
    return ::escapeHtml(toString(argument(0)));
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
        if ( !QDesktopServices::openUrl(QUrl(toString(argument(i)))) )
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
            args.append(toString(arg));
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
        return newVariant(values.first().value);

    QScriptValue result = m_engine->newObject();

    for (const auto &value : values)
        result.setProperty( value.name, newVariant(value.value) );

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
        QVariant value = settings.value(arg(0));
        if (value.canConvert<QStringList>())
            return toScriptValue(value.toStringList(), this);
        if (value.canConvert<QVariantMap>())
            return toScriptValue(value.value<QVariantMap>(), this);
        return newVariant(value);
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
    m_skipArguments = 2;

    QScriptValueList fnArgs;
    for (const auto &argument : arguments)
        fnArgs.append( newVariant(argument) );

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
                cmd = toString(fnArgs[skipArguments]);
                result = eval(cmd, "command-line");
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
    auto backtrace = m_engine->uncaughtExceptionBacktrace().join("\n");
    if ( !backtrace.isEmpty() )
        backtrace = "\n--- backtrace ---\n" + backtrace + "\n--- end backtrace ---";

    const auto exceptionText = exceptionName + backtrace;

    logScriptError(
                QString("Exception in command \"%1\": %2")
                .arg(cmd, exceptionText) );

    showExceptionMessage(exceptionText);

    return exceptionText;
}

void Scriptable::showExceptionMessage(const QString &message)
{
    if (!m_proxy)
        return;

    const auto title = m_actionName.isEmpty()
        ? tr("Exception")
        : tr("Exception in %1").arg( quoteString(m_actionName) );
    m_proxy->showMessage(title, message, QSystemTrayIcon::Warning, 8000);
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

        return m_proxy->getClipboardData(mime) != value;
    }

    if (args == 1) {
        QScriptValue value = argument(0);
        setTextData( &data, toString(value) );
        return setClipboard(&data, mode);
    }

    if (args % 2 == 0) {
        for (int i = 0; i < args; ++i) {
            // MIME
            QString mime = toString(argument(i));

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
        const QString mime = toString(argument(i));
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

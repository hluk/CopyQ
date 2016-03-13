/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "common/command.h"
#include "common/commandstatus.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "common/sleeptimer.h"
#include "common/version.h"
#include "item/serialize.h"
#include "scriptable/commandhelp.h"
#include "scriptable/dirclass.h"
#include "scriptable/fileclass.h"
#include "../qt/bytearrayclass.h"
#include "../qxt/qxtglobal.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSettings>
#include <QUrl>

Q_DECLARE_METATYPE(QByteArray*)
Q_DECLARE_METATYPE(QFile*)

namespace {

const char *const programName = "CopyQ Clipboard Manager";

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

QScriptValue getValue(QScriptEngine *eng, const QString &variableName)
{
    return eng->globalObject().property(variableName);
}

template <typename T>
T getValue(QScriptEngine *eng, const QString &variableName, T defaultValue)
{
    QVariant val = getValue(eng, variableName).toVariant();
    if ( val.canConvert<T>() )
        return val.value<T>();
    else
        return defaultValue;
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
        for ( int i = 0; i < list.size(); ++i )
            array.setProperty(i, ScriptValueFactory<T>::create(list[i], scriptable));
        return array;
    }
};

template <>
struct ScriptValueFactory<QVariantMap> {
    static QScriptValue create(const QVariantMap &dataMap, Scriptable *scriptable)
    {
        QScriptValue value = scriptable->engine()->newObject();

        foreach ( const QString &format, dataMap.keys() )
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
        if (scriptable.isAborted())
            return QByteArray();
        waitFor(100);
        if ( reply->waitForReadyRead(100) )
            data.append(reply->readAll());
    }
    data.append(reply->readAll());

    return data;
}

} // namespace

Scriptable::Scriptable(ScriptableProxy *proxy, QObject *parent)
    : QObject(parent)
    , QScriptable()
    , m_proxy(proxy)
    , m_engine(NULL)
    , m_baClass(NULL)
    , m_dirClass(NULL)
    , m_fileClass(NULL)
    , m_inputSeparator("\n")
    , m_input()
    , m_abort(false)
{
}

void Scriptable::initEngine(
        QScriptEngine *eng, const QString &currentPath, const QVariantMap &data)
{
    m_data = data;

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

    m_fileClass = new FileClass(currentPath, eng);
    addScriptableClass(&obj, m_fileClass);

    m_dirClass = new DirClass(currentPath, eng);
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
    return (bytes == NULL) ? value.toString()
                           : getTextData(*bytes);
}

bool Scriptable::toInt(const QScriptValue &value, int &number) const
{
    bool ok;
    number = toString(value).toInt(&ok);
    if (!ok)
        return false;

    return true;
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
    return NULL;
}

QByteArray Scriptable::makeByteArray(const QScriptValue &value) const
{
    QByteArray *data = getByteArray(value);
    return data ? *data : fromString(value.toString());
}

QFile *Scriptable::getFile(const QScriptValue &value) const
{
    if (value.scriptClass() == m_fileClass)
        return qscriptvalue_cast<QFile*>(value.data());
    return NULL;
}

bool Scriptable::toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const
{
    if (mime == mimeItems) {
        const QByteArray *itemData = getByteArray(value);
        if (!itemData)
            return false;

        return deserializeData(data, *itemData);
    }

    if (!mime.startsWith("text/") && value.scriptClass() == m_baClass)
        data->insert( mime, *getByteArray(value) );
    else
        data->insert( mime, toString(value).toUtf8() );

    return true;
}

QScriptValue Scriptable::applyRest(int first)
{
    if ( first >= context()->argumentCount() )
        return QScriptValue();

    QScriptValue fn = context()->argument(first);
    QString name = toString(fn);
    fn = getValue(engine(), name);
    if ( !fn.isFunction() ) {
        throwError( tr("Name \"%1\" doesn't refer to a function.").arg(name).toUtf8() );
        return QScriptValue();
    }

    QScriptValueList args;
    for (int i = first + 1; i < context()->argumentCount(); ++i)
        args.append( context()->argument(i) );

    return fn.call(QScriptValue(), args);
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
    context()->throwError( fromString(errorMessage + '\n') );
}

void Scriptable::sendMessageToClient(const QByteArray &message, int exitCode)
{
    emit sendMessage(message, exitCode);
}

void Scriptable::sendWindowActivationCommandToClient(const QByteArray &message)
{
    if ( !message.isEmpty() )
        sendMessageToClient(message, CommandActivateWindow);
}

QScriptValue Scriptable::version()
{
    return tr(programName) + " " COPYQ_VERSION " (hluk@email.cz)\n"
            + tr("Built with: ")
            + "Qt " + QT_VERSION_STR +
            + ", LibQxt " + QXT_VERSION_STR
            + '\n';
}

QScriptValue Scriptable::help()
{
    QString helpString;

    if ( argumentCount() == 0 ) {
        helpString.append(helpHead() + "\n");

        foreach (const CommandHelp &hlp, commandHelp())
            helpString.append(hlp.toString());

        helpString.append("\n" + helpTail() + "\n\n" + tr(programName)
            + " " + COPYQ_VERSION + " (hluk@email.cz)\n");
    } else {
        for (int i = 0; i < argumentCount(); ++i) {
            const QString &cmd = toString(argument(i));
            foreach (const CommandHelp &helpItem, commandHelp()) {
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
    if (argumentCount() == 0) {
        m_proxy->showWindow();
    } else if (argumentCount() == 1) {
        m_proxy->showBrowser(toString(argument(0)));
    } else {
        throwError(argumentError());
        return;
    }

    sendWindowActivationCommandToClient( m_proxy->mainWinId() );
}

void Scriptable::showAt()
{
    QRect rect(-1, -1, 0, 0);
    int n;
    int i = 0;
    if ( toInt(argument(i++), n) )
        rect.setX(n);
    if ( toInt(argument(i++), n) )
        rect.setY(n);
    if ( toInt(argument(i++), n) )
        rect.setWidth(n);
    if ( toInt(argument(i++), n) )
        rect.setHeight(n);

    const QString tabName = arg(i++);
    if (tabName.isEmpty())
        m_proxy->showWindowAt(rect);
    else
        m_proxy->showBrowserAt(tabName, rect);

    sendWindowActivationCommandToClient( m_proxy->mainWinId() );
}

void Scriptable::hide()
{
    m_proxy->close();
}

QScriptValue Scriptable::toggle()
{
    if ( m_proxy->toggleVisible() ) {
        sendWindowActivationCommandToClient( m_proxy->mainWinId() );
        return true;
    }

    return false;
}

void Scriptable::menu()
{
    bool shown = false;

    if (argumentCount() == 0) {
        shown = m_proxy->toggleMenu();
    } else if (argumentCount() == 1) {
        shown = m_proxy->toggleMenu(toString(argument(0)));
    } else {
        throwError(argumentError());
        return;
    }

    if (shown)
        sendWindowActivationCommandToClient( m_proxy->mainWinId() );
}

void Scriptable::exit()
{
    QByteArray message = fromString( tr("Terminating server.\n") );
    sendMessageToClient(message, CommandFinished);
    emit requestApplicationQuit();
}

void Scriptable::disable()
{
    m_proxy->disableMonitoring(true);
}

void Scriptable::enable()
{
    m_proxy->disableMonitoring(false);
}

QScriptValue Scriptable::monitoring()
{
    return m_proxy->isMonitoringEnabled();
}

QScriptValue Scriptable::visible()
{
    return m_proxy->isMainWindowVisible();
}

QScriptValue Scriptable::focused()
{
    return m_proxy->isMainWindowFocused();
}

void Scriptable::filter()
{
    m_proxy->filter(arg(0));
}

void Scriptable::ignore()
{
    m_proxy->abortAutomaticCommands();
}

QScriptValue Scriptable::clipboard()
{
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime) );
}

QScriptValue Scriptable::selection()
{
#ifdef COPYQ_WS_X11
    const QString &mime = arg(0, mimeText);
    return newByteArray( m_proxy->getClipboardData(mime, QClipboard::Selection) );
#else
    return QScriptValue();
#endif
}

QScriptValue Scriptable::copy()
{
    return copy(QClipboard::Clipboard);
}

QScriptValue Scriptable::copySelection()
{
#ifdef COPYQ_WS_X11
    return copy(QClipboard::Selection);
#else
    return QScriptValue();
#endif
}

void Scriptable::paste()
{
    m_proxy->pasteToCurrentWindow();
}

QScriptValue Scriptable::tab()
{
    const QString &name = arg(0);
    if ( name.isNull() )
        return toScriptValue( m_proxy->tabs(), this );

    m_proxy->setTab(name);
    return applyRest(1);
}

void Scriptable::removeTab()
{
    const QString &name = arg(0);
    const QString error = m_proxy->removeTab(name);
    if ( !error.isEmpty() )
        throwError(error);
}

void Scriptable::renameTab()
{
    const QString &name = arg(0);
    const QString &newName = arg(1);
    const QString error = m_proxy->renameTab(newName, name);
    if ( !error.isEmpty() )
        throwError(error);
}

QScriptValue Scriptable::tabIcon()
{
    if (argumentCount() == 1)
        return m_proxy->tabIcon(arg(0));

    if (argumentCount() == 2)
        m_proxy->setTabIcon(arg(0), arg(1));
    else
        throwError(argumentError());

    return QScriptValue();
}

QScriptValue Scriptable::length()
{
    return m_proxy->browserLength();
}

void Scriptable::select()
{
    QScriptValue value = argument(0);
    int row;
    if ( toInt(value, row) )
        m_proxy->browserMoveToClipboard(row);
}

void Scriptable::next()
{
    nextToClipboard(1);
}

void Scriptable::previous()
{
    nextToClipboard(-1);
}

void Scriptable::add()
{
    QStringList texts;

    for (int i = 0; i < argumentCount(); ++i)
        texts.append( toString(argument(i)) );

    m_proxy->browserAdd(texts);
}

void Scriptable::insert()
{
    int row;
    if ( !toInt(argument(0), row) ) {
        throwError(argumentError());
        return;
    }

    QScriptValue value = argument(1);
    QVariantMap data;
    setTextData( &data, toString(value) );
    m_proxy->browserAdd(data, row);
}

void Scriptable::remove()
{
    QList<int> rows = getRows();

    if (rows.size() != argumentCount()) {
        throwError(argumentError());
        return;
    }

    if ( rows.empty() )
        rows.append(0);

    m_proxy->browserRemoveRows(rows);
}

void Scriptable::edit()
{
    QScriptValue value;
    QString text;
    int row = -1;

    const int len = argumentCount();
    for ( int i = 0; i < len; ++i ) {
        value = argument(i);
        if (i > 0)
            text.append( getInputSeparator() );
        if ( toInt(value, row) ) {
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
    QByteArray result;
    QString mime(mimeText);
    QScriptValue value;
    QString sep = getInputSeparator();

    bool used = false;
    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, row) ) {
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
    changeItem(true);
}

void Scriptable::change()
{
    changeItem(false);
}

QScriptValue Scriptable::separator()
{
    setInputSeparator( toString(argument(0)) );
    return applyRest(1);
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
        if (!toInt(value, row))
            break;
        if (anyRows)
            text.append(sep);
        else
            anyRows = true;
        text.append( getTextData(m_proxy->browserItemData(row, mimeText)) );
    }

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
        sendWindowActivationCommandToClient(m_proxy->openActionDialog(data));
    }
}

void Scriptable::popup()
{
    QString title = arg(0);
    QString message = arg(1);
    int msec;
    if ( !toInt(argument(2), msec) )
        msec = 8000;
    m_proxy->showMessage(title, message, QSystemTrayIcon::Information, msec);
}

void Scriptable::exportTab()
{
    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_proxy->saveTab(getFileName(fileName)) ) {
        throwError( tr("Cannot save to file \"%1\"!").arg(fileName) );
    }
}

void Scriptable::importTab()
{
    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_proxy->loadTab(getFileName(fileName)) ) {
        throwError(
            tr("Cannot import file \"%1\"!").arg(fileName) );
    }
}

QScriptValue Scriptable::config()
{
    const QString name = arg(0);
    const QString value = arg(1);

    const QVariant result = m_proxy->config(name, value);

    if ( !result.isValid() )
        throwError( tr("Invalid option \"%1\"!").arg(name) );

    const QString output = result.toString();
    return output.isEmpty() ? QScriptValue() : output;
}

QScriptValue Scriptable::eval()
{
    const QString script = arg(0);

    const QScriptSyntaxCheckResult syntaxResult = engine()->checkSyntax(script);
    if (syntaxResult.state() != QScriptSyntaxCheckResult::Valid) {
        throwError( QString("Eval:%1:%2: syntax error: %3")
                    .arg(syntaxResult.errorLineNumber())
                    .arg(syntaxResult.errorColumnNumber())
                    .arg(syntaxResult.errorMessage()) );
        return QScriptValue();
    }

    return engine()->evaluate(script);
}

QScriptValue Scriptable::currentPath()
{
    if (argumentCount() > 0)
        setCurrentPath(arg(0));

    return getCurrentPath();
}

QScriptValue Scriptable::str(const QScriptValue &value)
{
    return toString(value);
}

QScriptValue Scriptable::input()
{
    if ( !getByteArray(m_input) ) {
        sendMessageToClient(QByteArray(), CommandReadInput);
        while ( !getByteArray(m_input) && !m_abort )
            QApplication::processEvents();
    }

    return m_input;
}

QScriptValue Scriptable::data(const QScriptValue &value)
{
    return newByteArray( m_data.value(toString(value)).toByteArray() );
}

QScriptValue Scriptable::setData()
{
    const QString mime = arg(0);
    if ( !toItemData(argument(1), mime, &m_data) )
        return false;

    if ( m_data.value(mimeSelectedItems).isValid() )
        m_proxy->setSelectedItemsData(mime, m_data.value(mime));

    return true;
}

void Scriptable::print(const QScriptValue &value)
{
    sendMessageToClient(makeByteArray(value), CommandSuccess);
}

void Scriptable::abort()
{
    if (m_abort)
        return;

    m_abort = true;

    QScriptEngine *eng = engine() ? engine() : m_engine;
    if (eng) {
        setInput(QByteArray()); // stop waiting for input
        eng->abortEvaluation();
    }
}

void Scriptable::fail()
{
    sendMessageToClient("", CommandError);
}

void Scriptable::keys()
{
#ifdef HAS_TESTS
    for (int i = 0; i < argumentCount(); ++i) {
        const QString keys = toString(argument(i));

        waitFor(500);
        const QString error = m_proxy->sendKeys(keys);
        if ( !error.isEmpty() ) {
            throwError(error);
            return;
        }

        // Make sure all keys are send (shortcuts are postponed because they can be blocked by modal windows).
        m_proxy->sendKeys("FLUSH_KEYS");
    }
#endif
}

QScriptValue Scriptable::testSelected()
{
    return m_proxy->testSelected();
}

void Scriptable::setCurrentTab()
{
    const QString tabName = arg(0);
    m_proxy->setCurrentTab(tabName);
}

QScriptValue Scriptable::selectItems()
{
    QList<int> rows = getRows();

    if (rows.size() != argumentCount()) {
        throwError(argumentError());
        return false;
    }

    return m_proxy->selectItems(rows);
}

QScriptValue Scriptable::selectedTab()
{
    return m_data.value(mimeCurrentTab).toString();
}

QScriptValue Scriptable::selectedItems()
{
    return toScriptValue( m_proxy->selectedItems(), this );
}

QScriptValue Scriptable::currentItem()
{
    return m_proxy->currentItem();
}

QScriptValue Scriptable::index()
{
    return currentItem();
}

QScriptValue Scriptable::escapeHtml()
{
    return ::escapeHtml(toString(argument(0)));
}

QScriptValue Scriptable::unpack()
{
    QVariantMap data;

    if ( !toItemData(argument(0), mimeItems, &data) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    return toScriptValue(data, this);
}

QScriptValue Scriptable::pack()
{
    QVariantMap data = toDataMap( argument(0) );
    return newByteArray(serializeData(data));
}

QScriptValue Scriptable::getItem()
{
    int row;
    if ( !toInt(argument(0), row) ) {
        throwError(argumentError());
        return QScriptValue();
    }

    return toScriptValue( m_proxy->browserItemData(row), this );
}

void Scriptable::setItem()
{
    int row;
    if ( !toInt(argument(0), row) ) {
        throwError(argumentError());
        return;
    }

    QVariantMap data = toDataMap( argument(1) );
    m_proxy->browserAdd(data, row);
}

QScriptValue Scriptable::toBase64()
{
    return QString::fromLatin1(makeByteArray(argument(0)).toBase64());
}

QScriptValue Scriptable::fromBase64()
{
    return newByteArray(QByteArray::fromBase64(makeByteArray(argument(0))));
}

QScriptValue Scriptable::open()
{
    for ( int i = 0; i < argumentCount(); ++i ) {
        if ( !QDesktopServices::openUrl(QUrl(toString(argument(i)))) )
            return false;
    }

    return true;
}

QScriptValue Scriptable::execute()
{
    // Pass all arguments until null to command. The rest will be sent to stdin.
    QStringList args;
    int i = 0;
    for ( ; i < argumentCount(); ++i ) {
        const QScriptValue arg = argument(i);
        if (arg.isNull())
            break;
        args.append(toString(arg));
    }

    Action action;
    for ( ++i ; i < argumentCount(); ++i )
        action.setInput( action.input() + makeByteArray(argument(i)) );

    action.setCommand(args);
    action.setOutputFormat("DATA");
    action.start();

    if ( !action.waitForStarted(5000) )
        return QScriptValue();

    while ( !action.waitForFinished(5000) && !m_abort ) {}

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
    return m_proxy->currentWindowTitle();
}

QScriptValue Scriptable::dialog()
{
    NamedValueList values;
    values.reserve(argumentCount() / 2);

    for ( int i = 0; i < argumentCount(); i += 2 ) {
        const QString key = arg(i);
        const QScriptValue value = argument(i + 1);
        QByteArray *bytes = getByteArray(value);
        if (bytes) {
            values.append( NamedValue(key, QVariant(*bytes)) );
        } else {
            QFile *file = getFile(value);
            if (file)
                values.append( NamedValue(key, QVariant::fromValue(file)) );
            else
                values.append( NamedValue(key, value.toVariant()) );
        }
    }

    values = m_proxy->inputDialog(values);

    if (values.isEmpty())
        return QScriptValue();

    if (values.size() == 1)
        return newVariant(values.first().value);

    QScriptValue result = m_engine->newObject();

    foreach (const NamedValue &value, values)
        result.setProperty( value.name, newVariant(value.value) );

    return result;
}

QScriptValue Scriptable::settings()
{
    if (argumentCount() == 2) {
        const QString key = arg(0);
        const QScriptValue value = argument(1);
        const QByteArray *bytes = getByteArray(value);
        const QVariant saveValue = bytes ? QVariant(*bytes) : value.toVariant();
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
    const QDateTime dateTime = QDateTime::currentDateTime();
    return dateTime.toString(arg(0));
}

void Scriptable::updateFirst()
{
    m_proxy->updateFirstItem(m_data);
}

void Scriptable::updateTitle()
{
    if (argumentCount() == 0)
        m_proxy->updateTitle(m_data);
    else
        m_proxy->updateTitle(QVariantMap());
}

QScriptValue Scriptable::networkGet()
{
    const QString url = arg(0);
    return NetworkReply::get(url, this);
}

QScriptValue Scriptable::networkPost()
{
    const QString url = arg(0);
    const QByteArray postData = makeByteArray(argument(1));
    return NetworkReply::post(url, postData, this);
}

QScriptValue Scriptable::env()
{
    const QString name = arg(0);
    const QByteArray value = qgetenv(name.toUtf8().constData());
    return newByteArray(value);
}

QScriptValue Scriptable::setEnv()
{
    const QString name = arg(0);
    const QByteArray value = makeByteArray(argument(1));
    return qputenv(name.toUtf8().constData(), value);
}

void Scriptable::setInput(const QByteArray &bytes)
{
    m_input = newByteArray(bytes);
}

QList<int> Scriptable::getRows() const
{
    QList<int> rows;

    for ( int i = 0; i < argumentCount(); ++i ) {
        int row;
        if ( toInt(argument(i), row) )
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
        if ( !setClipboard(data, mode) )
            return false;

        m_proxy->copyFromCurrentWindow();

        return m_proxy->getClipboardData(mime) != value;
    }

    if (args == 1) {
        QScriptValue value = argument(0);
        setTextData( &data, toString(value) );
        return setClipboard(data, mode);
    }

    if (args % 2 == 0) {
        for (int i = 0; i < args; ++i) {
            // MIME
            QString mime = toString(argument(i));

            // DATA
            toItemData(argument(++i), mime, &data);
        }

        return setClipboard(data, mode);
    }

    throwError(argumentError());
    return false;
}

bool Scriptable::setClipboard(QVariantMap &data, QClipboard::Mode mode)
{
    const QString mime = COPYQ_MIME_PREFIX "hash";
    data.remove(mime);
    const QByteArray id = QByteArray::number(hash(data));
    data.insert(mime, id);

    m_proxy->setClipboard(data, mode);

    // Wait for clipboard to be set.
    for (int i = 0; i < 30; ++i) {
        waitFor(100);
        if ( clipboardContains(mode, m_proxy, mime, id) )
            return true;
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
    if ( toInt(argument(0), row) ) {
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

    if (create)
        m_proxy->browserAdd(data, row);
    else
        m_proxy->browserChange(data, row);
}

void Scriptable::nextToClipboard(int where)
{
    QVariantMap data = m_proxy->nextItem(where);
    if (data.isEmpty())
        return;

    setClipboard(data, QClipboard::Clipboard);
#ifdef COPYQ_WS_X11
    setClipboard(data, QClipboard::Selection);
#endif
}

QScriptValue NetworkReply::get(const QString &url, Scriptable *scriptable)
{
    NetworkReply *reply = new NetworkReply(url, QByteArray(), scriptable);
    return reply->toScriptValue();
}

QScriptValue NetworkReply::post(const QString &url, const QByteArray &postData, Scriptable *scriptable)
{
    NetworkReply *reply = new NetworkReply(url, postData, scriptable);
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
    foreach ( const QByteArray &header, m_replyHead->rawHeaderList() ) {
        QScriptValue pair = m_scriptable->engine()->newArray();
        pair.setProperty( 0, m_scriptable->newByteArray(header) );
        pair.setProperty( 1, m_scriptable->newByteArray(m_replyHead->rawHeader(header)) );
        headers.setProperty( i++, pair );
    }

    return headers;
}

NetworkReply::NetworkReply(const QString &url, const QByteArray &postData, Scriptable *scriptable)
    : QObject(scriptable)
    , m_scriptable(scriptable)
    , m_manager(new QNetworkAccessManager(this))
    , m_reply(NULL)
    , m_replyHead(NULL)
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

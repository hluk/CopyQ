/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "client_server.h"
#include "clipboardbrowser.h"
#include "clipboarditem.h"
#include "configurationmanager.h"
#include "mainwindow.h"
#include "scriptable.h"
#include "../qt/bytearrayclass.h"
#include "../qxt/qxtglobal.h"
#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QDir>
#include <QLocalSocket>
#include <QMimeData>
#include <QScriptContext>
#include <QScriptEngine>

Q_DECLARE_METATYPE(QByteArray*)

namespace {

const QString defaultMime("text/plain");

const char *const programName = "CopyQ Clipboard Manager";

const QString nl("\n");

struct CommandHelp {
    CommandHelp()
        : cmd()
        , desc()
        , args()
    {
    }

    CommandHelp(const char *command, const QString &description)
        : cmd(command)
        , desc(description)
        , args()
    {
    }

    CommandHelp &addArg(const QString &arg)
    {
        args.append(' ');
        args.append(arg);
        return *this;
    }

    QString toString() const
    {
        if (cmd.isNull())
            return nl;

        const int indent = 23;
        bool indentFirst = desc.startsWith('\n');
        return QString("    %1").arg(cmd + args, indentFirst ? 0 : -indent)
                + (indentFirst ? QString() : QString("  "))
                + QString(desc).replace('\n', "\n" + QString(' ')
                    .repeated(4 + 2 + (indentFirst ? 0 : indent))) + nl;
    }

    QString cmd;
    QString desc;
    QString args;
};

QList<CommandHelp> commandHelp()
{
    return QList<CommandHelp>()
        << CommandHelp("show",
                       Scriptable::tr("Show main window."))
        << CommandHelp("hide",
                       Scriptable::tr("Hide main window."))
        << CommandHelp("toggle",
                       Scriptable::tr("Show or hide main window."))
        << CommandHelp("menu",
                       Scriptable::tr("Open context menu."))
        << CommandHelp("exit",
                       Scriptable::tr("Exit server."))
        << CommandHelp("disable, enable",
                       Scriptable::tr("Disable or enable clipboard content storing."))
        << CommandHelp()
        << CommandHelp("clipboard",
                       Scriptable::tr("Print clipboard content."))
           .addArg("[" + Scriptable::tr("MIME") + "]")
    #ifdef COPYQ_WS_X11
        << CommandHelp("selection",
                       Scriptable::tr("Print X11 selection content."))
           .addArg("[" + Scriptable::tr("MIME") + "]")
    #endif
        << CommandHelp("paste",
                       Scriptable::tr("Paste clipboard to current window\n"
                                      "(may not work with some applications)."))
        << CommandHelp("copy", Scriptable::tr("Set clipboard text."))
           .addArg(Scriptable::tr("TEXT"))
        << CommandHelp("copy", Scriptable::tr("\nSet clipboard content."))
           .addArg(Scriptable::tr("MIME"))
           .addArg(Scriptable::tr("DATA"))
           .addArg("[" + Scriptable::tr("MIME") + " " + Scriptable::tr("DATA") + "]...")
        << CommandHelp()
        << CommandHelp("length, count, size",
                       Scriptable::tr("Print number of items in history."))
        << CommandHelp("select",
                       Scriptable::tr("Copy item in the row to clipboard."))
           .addArg("[" + Scriptable::tr("ROW") + "=0]")
        << CommandHelp("next",
                       Scriptable::tr("Copy next item from current tab to clipboard."))
        << CommandHelp("previous",
                       Scriptable::tr("Copy previous item from current tab to clipboard."))
        << CommandHelp("add",
                       Scriptable::tr("Add text into clipboard."))
           .addArg(Scriptable::tr("TEXT") + "...")
        << CommandHelp("insert",
                       Scriptable::tr("Insert text into given row."))
           .addArg(Scriptable::tr("ROW"))
           .addArg(Scriptable::tr("TEXT"))
        << CommandHelp("remove",
                       Scriptable::tr("Remove items in given rows."))
           .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
        << CommandHelp("edit",
                       Scriptable::tr("Edit items or edit new one.\n"
                                   "Value -1 is for current text in clipboard."))
           .addArg("[" + Scriptable::tr("ROWS") + "...]")
        << CommandHelp()
        << CommandHelp("separator",
                       Scriptable::tr("Set separator for items on output."))
           .addArg(Scriptable::tr("SEPARATOR"))
        << CommandHelp("read",
                       Scriptable::tr("Print raw data of clipboard or item in row."))
           .addArg("[" + Scriptable::tr("MIME") + "|" + Scriptable::tr("ROW") + "]...")
        << CommandHelp("write", Scriptable::tr("\nWrite raw data to given row."))
           .addArg("[" + Scriptable::tr("ROW") + "=0]")
           .addArg(Scriptable::tr("MIME"))
           .addArg(Scriptable::tr("DATA"))
           .addArg("[" + Scriptable::tr("MIME") + " " + Scriptable::tr("DATA") + "]...")
        << CommandHelp()
        << CommandHelp("action",
                       Scriptable::tr("Show action dialog."))
           .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
        << CommandHelp("action",
                       Scriptable::tr("\nRun PROGRAM on item text in the rows.\n"
                                   "Use %1 in PROGRAM to pass text as argument."))
           .addArg("[" + Scriptable::tr("ROWS") + "=0...]")
           .addArg("[" + Scriptable::tr("PROGRAM") + " [" + Scriptable::tr("SEPARATOR") + "=\\n]]")
        << CommandHelp("popup",
                       Scriptable::tr("\nShow tray popup message for TIME milliseconds."))
           .addArg(Scriptable::tr("TITLE"))
           .addArg(Scriptable::tr("MESSAGE"))
           .addArg("[" + Scriptable::tr("TIME") + "=8000]")
        << CommandHelp()
        << CommandHelp("tab",
                       Scriptable::tr("List available tab names."))
        << CommandHelp("tab",
                       Scriptable::tr("Run command on tab with given name.\n"
                                   "Tab is created if it doesn't exist.\n"
                                   "Default is the first tab."))
           .addArg(Scriptable::tr("NAME"))
           .addArg("[" + Scriptable::tr("COMMAND") + "]")
        << CommandHelp("removetab",
                       Scriptable::tr("Remove tab."))
           .addArg(Scriptable::tr("NAME"))
        << CommandHelp("renametab",
                       Scriptable::tr("Rename tab."))
           .addArg(Scriptable::tr("NAME"))
           .addArg(Scriptable::tr("NEW_NAME"))
        << CommandHelp()
        << CommandHelp("exporttab",
                       Scriptable::tr("Export items to file."))
           .addArg(Scriptable::tr("FILE_NAME"))
        << CommandHelp("importtab",
                       Scriptable::tr("Import items from file."))
           .addArg(Scriptable::tr("FILE_NAME"))
        << CommandHelp()
        << CommandHelp("config",
                       Scriptable::tr("List all options."))
        << CommandHelp("config",
                       Scriptable::tr("Get option value."))
           .addArg(Scriptable::tr("OPTION"))
        << CommandHelp("config",
                       Scriptable::tr("Set option value."))
           .addArg(Scriptable::tr("OPTION"))
           .addArg(Scriptable::tr("VALUE"))
        << CommandHelp()
        << CommandHelp("eval, -e",
                       Scriptable::tr("Evaluate ECMAScript program."))
           .addArg("[" + Scriptable::tr("SCRIPT") + "]")
        << CommandHelp("help, -h, --help",
                       Scriptable::tr("\nPrint help for COMMAND or all commands."))
           .addArg("[" + Scriptable::tr("COMMAND") + "]")
        << CommandHelp("version, -v, --version",
                       Scriptable::tr("\nPrint version of program and libraries."))
#ifdef HAS_TESTS
        << CommandHelp("tests, --tests",
                       Scriptable::tr("Run tests."))
#endif
           ;
}

QString helpHead()
{
    return Scriptable::tr("Usage: copyq [%1]").arg(Scriptable::tr("COMMAND")) + nl
        + nl
        + Scriptable::tr("Starts server if no command is specified.") + nl
        + Scriptable::tr("  COMMANDs:");
}

QString helpTail()
{
    return Scriptable::tr("NOTES:") + nl
        + Scriptable::tr("  - Use dash argument (-) to read data from stdandard input.") + nl
        + Scriptable::tr("  - Use double-dash argument (--) to read all following arguments without\n"
                      "    expanding escape sequences (i.e. \\n, \\t and others).") + nl
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

} // namespace

Scriptable::Scriptable(MainWindow *wnd, QLocalSocket *client, QObject *parent)
    : QObject(parent)
    , QScriptable()
    , m_wnd(wnd)
    , m_client(client)
    , m_engine(NULL)
    , m_baClass(NULL)
    , m_currentTab()
    , m_inputSeparator("\n")
    , m_currentPath()
{
}

void Scriptable::initEngine(QScriptEngine *eng, const QString &currentPath)
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
    eng->setGlobalObject(obj);
    eng->setProcessEventsInterval(100);

    m_baClass = new ByteArrayClass(eng);
    obj.setProperty( "ByteArray", m_baClass->constructor() );

    setCurrentPath(currentPath);
}

QScriptValue Scriptable::newByteArray(const QByteArray &bytes)
{
    return m_baClass->newInstance(bytes);
}

QString Scriptable::toString(const QScriptValue &value) const
{
    QByteArray *bytes = toByteArray(value);
    return (bytes == NULL) ? value.toString()
                           : QString::fromLocal8Bit( bytes->data() );
}

bool Scriptable::toInt(const QScriptValue &value, int &number) const
{
    bool ok;
    number = toString(value).toInt(&ok);
    if (!ok)
        return false;

    return true;
}

QByteArray *Scriptable::toByteArray(const QScriptValue &value) const
{
    if (value.scriptClass() == m_baClass)
        return qscriptvalue_cast<QByteArray*>(value.data());
    else
        return NULL;
}

QScriptValue Scriptable::applyRest(int first)
{
    if ( first >= context()->argumentCount() )
        return QScriptValue();

    QScriptValue fn = context()->argument(first);
    QString name = toString(fn);
    fn = getValue(engine(), name);
    if ( !fn.isFunction() ) {
        throwError( tr("Name \"%1\" doesn't refer to a function.").arg(name) );
        return QScriptValue();
    }

    QScriptValueList args;
    for (int i = first + 1; i < context()->argumentCount(); ++i)
        args.append( context()->argument(i) );

    return fn.call(QScriptValue(), args);
}

ClipboardBrowser *Scriptable::currentTab()
{
    if ( m_currentTab.isEmpty() )
        return m_wnd->browser(0);

    int i = m_wnd->findTabIndex(m_currentTab);
    return (i != -1) ? m_wnd->browser(i) : m_wnd->createTab(m_currentTab, true);
}

const QString &Scriptable::getCurrentTab() const
{
    return m_currentTab;
}

void Scriptable::setCurrentTab(const QString &tab)
{
    m_currentTab = tab;
}

const QString &Scriptable::getInputSeparator() const
{
    return m_inputSeparator;
}

void Scriptable::setInputSeparator(const QString &separator)
{
    m_inputSeparator = separator;
}

const QString &Scriptable::getCurrentPath() const
{
    return m_currentPath;
}

void Scriptable::setCurrentPath(const QString &path)
{
    m_currentPath = path;
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

void Scriptable::sendMessage(const QByteArray &message, int exitCode)
{
    if (m_client == NULL)
        return;
    QByteArray msg;
    QDataStream out(&msg, QIODevice::WriteOnly);
    out << exitCode;
    out.writeRawData( message.constData(), message.length() );
    writeMessage(m_client, msg);
}

void Scriptable::throwError(const QString &errorMessage)
{
    context()->throwError( errorMessage.toLocal8Bit() + QString("\n") );
}

QScriptValue Scriptable::version()
{
    return tr(programName) + " v" COPYQ_VERSION " (hluk@email.cz)\n"
            + tr("Build with: ")
            + "Qt " + QT_VERSION_STR +
            + ", LibQxt " + QXT_VERSION_STR
            + '\n';
}

QScriptValue Scriptable::help()
{
    const QString &cmd = arg(0);
    QString helpString;

    if (cmd.isNull())
        helpString.append(helpHead() + nl);

    bool found = cmd.isNull();
    foreach (CommandHelp hlp, commandHelp()) {
        if (cmd.isNull()) {
            helpString.append(hlp.toString());
        } else if (hlp.cmd.contains(cmd)) {
            found = true;
            helpString.append(hlp.toString());
        }
    }

    if (!found) {
        throwError( tr("Command not found!") );
        return QString();
    }

    if (cmd.isNull())
        helpString.append(nl + helpTail());

    return helpString.toLocal8Bit() +
            (cmd.isNull() ? nl + nl + tr(programName) + " v" + COPYQ_VERSION + " (hluk@email.cz)\n"
                          : QString());
}

void Scriptable::show()
{
    m_wnd->showWindow();
    QByteArray message = QByteArray::number((qlonglong)m_wnd->winId());
    sendMessage(message, CommandActivateWindow);
}

void Scriptable::hide()
{
    m_wnd->close();
}

void Scriptable::toggle()
{
    m_wnd->toggleVisible();
    if (m_wnd->isVisible()) {
        QByteArray message = QByteArray::number((qlonglong)m_wnd->winId());
        sendMessage(message, CommandActivateWindow);
    }
}

void Scriptable::menu()
{
    m_wnd->toggleMenu();
    if (m_wnd->isTrayMenuVisible()) {
        QByteArray message = QByteArray::number((qlonglong)m_wnd->trayMenuWinId());
        sendMessage(message, CommandActivateWindow);
    }
}

void Scriptable::exit()
{
    // Exit application - respond to client first.
    if (m_client == NULL)
        return;
    QByteArray message = QByteArray(tr("Terminating server.\n").toLatin1());
    sendMessage(message, CommandSuccess);
    m_client->flush();
    QApplication::exit(0);
}

void Scriptable::disable()
{
    m_wnd->disableMonitoring(true);
}

void Scriptable::enable()
{
    m_wnd->disableMonitoring(false);
}

QScriptValue Scriptable::clipboard()
{
    const QString &mime = arg(0, defaultMime);
    const QMimeData *data = clipboardData();
    if (data) {
        if (mime == "?")
            return data->formats().join("\n") + '\n';
        else
            return newByteArray( data->data(mime) );
    }

    return QScriptValue();
}

QScriptValue Scriptable::selection()
{
#ifdef COPYQ_WS_X11
    const QString &mime = arg(0, defaultMime);
    const QMimeData *data = clipboardData(QClipboard::Selection);
    if (data) {
        if (mime == "?")
            return data->formats().join("\n") + '\n';
        else
            return newByteArray( data->data(mime) );
    }
#endif

    return QScriptValue();
}

void Scriptable::copy()
{
    int args = argumentCount();

    ClipboardItem item;
    if (args == 1) {
        QScriptValue value = argument(0);
        QByteArray *bytes = toByteArray(value);
        item.setData( defaultMime, bytes != NULL ? *bytes : toString(value).toLocal8Bit() );
    } else if (args % 2 == 1) {
        for (int i = 0; i < args; ++i) {
            // MIME
            QString mime = toString(argument(i));

            // DATA
            QScriptValue value = argument(++i);
            QByteArray *bytes = toByteArray(value);
            item.setData( mime, bytes != NULL ? *bytes : toString(value).toLocal8Bit() );
        }
    } else {
        throwError(argumentError());
        return;
    }

    m_wnd->setClipboard(&item);
}

void Scriptable::paste()
{
    PlatformPtr platform = createPlatformNativeInterface();
    platform->pasteToWindow(platform->getCurrentWindow());
}

QScriptValue Scriptable::tab()
{
    const QString &name = arg(0);
    if ( name.isNull() ) {
        QString response;
        foreach ( const QString &tabName, m_wnd->tabs() )
            response.append(tabName + '\n');
        return response;
    } else {
        setCurrentTab(name);
        return applyRest(1);
    }
}

void Scriptable::removetab()
{
    const QString &name = arg(0);
    int i = getTabIndexOrError(name);
    if (i != -1)
        m_wnd->removeTab(false, i);
}

void Scriptable::renametab()
{
    const QString &name = arg(0);
    const QString &newName = arg(1);
    int i = getTabIndexOrError(name);
    if (i == -1)
        return;
    else if ( newName.isEmpty() )
        throwError( tr("Tab name cannot be empty!") );
    else if ( m_wnd->tabs().indexOf(newName) >= 0 )
        throwError( tr("Tab with given name already exists!") );
    else
        m_wnd->renameTab(newName, i);
}

QScriptValue Scriptable::length()
{
    return currentTab()->length();
}

void Scriptable::select()
{
    ClipboardBrowser *c = currentTab();

    QScriptValue value = argument(0);
    int row;
    if ( toInt(value, row) ) {
        c->moveToClipboard(row);
        c->updateClipboard();
        c->delayedSaveItems(1000);
    }
}

void Scriptable::next()
{
    ClipboardBrowser *c = m_currentTab.isEmpty() ? m_wnd->browser()
                                                 : m_wnd->findTab(m_currentTab);
    if (c != NULL)
        c->copyNextItemToClipboard();
}

void Scriptable::previous()
{
    ClipboardBrowser *c = m_currentTab.isEmpty() ? m_wnd->browser()
                                                 : m_wnd->findTab(m_currentTab);
    if (c != NULL)
        c->copyPreviousItemToClipboard();
}

void Scriptable::add()
{
    ClipboardBrowser *c = currentTab();
    ClipboardBrowser::Lock lock(c);

    for (int i = 0; i < argumentCount(); ++i) {
        QScriptValue value = argument(i);
        QByteArray *bytes = toByteArray(value);
        if (bytes != NULL) {
            QMimeData *data = new QMimeData;
            data->setData(defaultMime, *bytes);
            c->add(data, true);
        } else {
            c->add( toString(value), true );
        }
    }

    c->updateClipboard();
    c->delayedSaveItems(1000);
}

void Scriptable::insert()
{
    ClipboardBrowser *c = currentTab();
    ClipboardBrowser::Lock lock(c);

    int row;
    if ( !toInt(argument(0), row) ) {
        throwError(argumentError());
        return;
    }

    QScriptValue value = argument(1);
    QByteArray *bytes = toByteArray(value);
    QMimeData *data = new QMimeData;
    data->setData( defaultMime, bytes != NULL ? *bytes : toString(value).toLocal8Bit() );
    c->add(data, true, row);

    if (row == 0)
        c->updateClipboard();
    c->delayedSaveItems(1000);
}

void Scriptable::remove()
{
    QScriptValue value;
    QList<int> rows;

    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, row) )
            rows.append(row);
    }

    if ( rows.empty() )
        rows.append(0);

    ClipboardBrowser *c = currentTab();
    ClipboardBrowser::Lock lock(c);

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    foreach (int row, rows)
        c->removeRow(row);

    if (rows.last() == 0)
        c->updateClipboard();
    c->delayedSaveItems(1000);
}

void Scriptable::edit()
{
    ClipboardBrowser *c = currentTab();
    QScriptValue value;
    QString text;
    int row;

    const int len = argumentCount();
    for ( int i = 0; i < len; ++i ) {
        value = argument(i);
        if (i > 0)
            text.append( getInputSeparator() );
        if ( toInt(value, row) ) {
            if (row >= 0) {
                text.append( c->itemText(row) );
            } else {
                const QMimeData *data = clipboardData();
                if (data != NULL)
                    text.append(data->text());
            }
        } else {
            text.append( toString(value) );
        }
    }

    if ( !c->openEditor(text.toLocal8Bit()) ) {
        m_wnd->showBrowser(c);
        if (len == 1 && row >= 0) {
            QModelIndex index = c->index(row);
            c->setCurrent(row);
            c->scrollTo(index, QAbstractItemView::PositionAtTop);
            c->edit(index);
        } else {
            c->editNew(text);
        }
    }
}

QScriptValue Scriptable::read()
{
    QByteArray result;
    QString mime(defaultMime);
    QScriptValue value;
    QString sep = getInputSeparator();
    const QMimeData *data;

    bool used = false;
    for ( int i = 0; i < argumentCount(); ++i ) {
        value = argument(i);
        int row;
        if ( toInt(value, row) ) {
            if (used)
                result.append(sep);
            used = true;
            data = (row >= 0) ?
                currentTab()->itemData(row) : clipboardData();
            if (data) {
                if (mime == "?")
                    result.append( data->formats().join("\n") );
                else
                    result.append( data->data(mime) );
            }
        } else {
            mime = toString(value);
        }
    }

    if (!used) {
        data = clipboardData();
        if (data == NULL)
            return QScriptValue();
        else if (mime == "?")
            result.append(data->formats().join("\n") + '\n');
        else
            result.append(data->data(mime));
    }

    return newByteArray(result);
}

void Scriptable::write()
{
    int arg = 0;
    QScriptValue value = argument(0);

    // [ROW]
    int row;
    int args = argumentCount();
    if ( toInt(value, row) ) {
        if (args < 3 || args % 2 != 1 ) {
            throwError(argumentError());
            return;
        }
        value = argument(++arg);
    } else {
        if (args < 2 || args % 2 != 0 ) {
            throwError(argumentError());
            return;
        }
        row = 0;
    }

    QMimeData *data = new QMimeData();

    while (arg < args) {
        // MIME
        QString mime = toString(value);

        // DATA
        value = argument(++arg);
        QByteArray *bytes = toByteArray(value);
        data->setData( mime, bytes != NULL ? *bytes : toString(value).toLocal8Bit() );

        value = argument(++arg);
    }

    ClipboardBrowser *c = currentTab();
    c->add(data, true, row);
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
    ClipboardBrowser *c = currentTab();
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
        text.append(c->itemText(row));
    }

    if (!anyRows) {
        const QMimeData *data = clipboardData();
        if (data != NULL)
            text = data->text();
    }

    if (i < argumentCount()) {
        Command command;
        command.cmd = toString(value);
        command.output = defaultMime;
        command.input = defaultMime;
        command.wait = false;
        command.outputTab = c->getID();
        command.sep = ((i + 1) < argumentCount()) ? toString( argument(i + 1) )
                                                  : QString('\n');
        QMimeData data;
        data.setText(text);
        m_wnd->action(data, command);
    } else {
        QMimeData data;
        data.setText(text);
        QByteArray message = QByteArray::number((qlonglong)m_wnd->openActionDialog(data));
        sendMessage(message, CommandActivateWindow);
    }
}

void Scriptable::popup()
{
    QString title = arg(0);
    QString message = arg(1);
    int msec;
    if ( !toInt(argument(2), msec) )
        msec = 8000;
    m_wnd->showMessage(title, message, QSystemTrayIcon::Information, msec);
}

void Scriptable::exporttab()
{
    const QString &fileName = arg(0);
    ClipboardBrowser *c = currentTab();
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_wnd->saveTab(getFileName(fileName), m_wnd->tabIndex(c)) ) {
        throwError(
            tr("Cannot save to file \"%1\"!").arg(fileName) );
    }
}

void Scriptable::importtab()
{
    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        throwError(argumentError());
    } else if ( !m_wnd->loadTab(getFileName(fileName)) ) {
        throwError(
            tr("Cannot import file \"%1\"!").arg(fileName) );
    }
}

QScriptValue Scriptable::config()
{
    const QString name = arg(0);
    const QString value = arg(1);
    ConfigurationManager *cm = ConfigurationManager::instance();

    if ( name.isNull() ) {
        // print options
        QStringList options = cm->options();
        options.sort();
        QString opts;
        foreach (const QString &option, options)
            opts.append( option + "\n  " +
                         cm->optionToolTip(option).replace('\n', "\n  ").toLocal8Bit() + '\n' );
        return opts;
    } else {
        if ( cm->options().contains(name) ) {
            if ( value.isNull() ) {
                // print option value
                return cm->value(name).toString();
            } else {
                // set option
                if ( cm->isVisible() ) {
                    throwError(
                        tr("To modify options from command line you must first "
                           "close the CopyQ Configuration dialog!") );
                } else {
                    cm->setValue(name, value);
                    cm->saveSettings();
                }
            }
        } else {
            throwError( tr("Invalid option!") );
        }
    }

    return QScriptValue();
}

void Scriptable::eval()
{
    const QString script = arg(0);
    engine()->evaluate(script);
}

void Scriptable::currentpath()
{
    const QString path = arg(0);
    setCurrentPath(path);
}

QScriptValue Scriptable::str(const QScriptValue &value)
{
    return toString(value);
}

void Scriptable::print(const QScriptValue &value)
{
    QByteArray *message = toByteArray(value);
    QByteArray bytes;
    if (message == NULL) {
        bytes = value.toString().toLocal8Bit();
        message = &bytes;
    }
    sendMessage(*message, CommandSuccess);
}

void Scriptable::abort()
{
    QScriptEngine *eng = engine();
    if (eng == NULL)
        eng = m_engine;
    if ( eng && eng->isEvaluating() )
        eng->abortEvaluation();
}

int Scriptable::getTabIndexOrError(const QString &name)
{
    int i = m_wnd->tabs().indexOf(name);
    if (i == -1)
        throwError( tr("Tab with given name doesn't exist!") );
    return i;
}

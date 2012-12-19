/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "mainwindow.h"
#include "client_server.h"
#include "clipboardbrowser.h"
#include "../qt/bytearrayclass.h"
#include "../qxt/qxtglobal.h"

#include <QLocalSocket>
#include <QScriptContext>
#include <QScriptEngine>
#include <QTimer>
#include <QApplication>
#include <QDir>

Q_DECLARE_METATYPE(QByteArray*)

namespace {

const char *const programName = "CopyQ Clipboard Manager";

const char *const versionString = "1.4.0";

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
                       QObject::tr("Show main window."))
        << CommandHelp("hide",
                       QObject::tr("Hide main window."))
        << CommandHelp("toggle",
                       QObject::tr("Show or hide main window."))
        << CommandHelp("menu",
                       QObject::tr("Open context menu."))
        << CommandHelp("exit",
                       QObject::tr("Exit server."))
        << CommandHelp()
        << CommandHelp("clipboard",
                       QObject::tr("Print clipboard content."))
           .addArg("[" + QObject::tr("MIME") + "]")
    #ifdef Q_WS_X11
        << CommandHelp("selection",
                       QObject::tr("Print X11 selection content."))
           .addArg("[" + QObject::tr("MIME") + "]")
    #endif
        << CommandHelp()
        << CommandHelp("length, count, size",
                       QObject::tr("Print number of items in history."))
        << CommandHelp("select",
                       QObject::tr("Move item in the row to clipboard."))
           .addArg("[" + QObject::tr("ROW") + "=0]")
        << CommandHelp("add",
                       QObject::tr("Add text into clipboard."))
           .addArg(QObject::tr("TEXT") + "...")
        << CommandHelp("remove",
                       QObject::tr("Remove items in given rows."))
           .addArg("[" + QObject::tr("ROWS") + "=0...]")
        << CommandHelp("edit",
                       QObject::tr("Edit items or edit new one.\n"
                                   "Value -1 is for current text in clipboard."))
           .addArg("[" + QObject::tr("ROWS") + "...]")
        << CommandHelp()
        << CommandHelp("read",
                       QObject::tr("Print raw data of clipboard or item in row."))
           .addArg("[" + QObject::tr("MIME") + "|" + QObject::tr("ROW") + "]...")
        << CommandHelp("write", QObject::tr("Write raw data to clipboard."))
           .addArg(QObject::tr("MIME"))
           .addArg(QObject::tr("DATA"))
        << CommandHelp("separator",
                       QObject::tr("Set separator for items on output."))
           .addArg(QObject::tr("SEPARATOR"))
        << CommandHelp()
        << CommandHelp("action",
                       QObject::tr("Show action dialog."))
           .addArg("[" + QObject::tr("ROWS") + "=0...]")
        << CommandHelp("action",
                       QObject::tr("\nRun PROGRAM on item text in the rows.\n"
                                   "Use %1 in PROGRAM to pass text as argument."))
           .addArg("[" + QObject::tr("ROWS") + "=0...]")
           .addArg("[" + QObject::tr("PROGRAM") + " [" + QObject::tr("SEPARATOR") + "=\\n]]")
        << CommandHelp("popup",
                       QObject::tr("\nShow tray popup message for TIME milliseconds."))
           .addArg(QObject::tr("TITLE"))
           .addArg(QObject::tr("MESSAGE"))
           .addArg("[" + QObject::tr("TIME") + "=8000]")
        << CommandHelp()
        << CommandHelp("tab",
                       QObject::tr("List available tab names."))
        << CommandHelp("tab",
                       QObject::tr("Run command on tab with given name.\n"
                                   "Tab is created if it doesn't exist.\n"
                                   "Default is the first tab."))
           .addArg(QObject::tr("NAME"))
           .addArg("[" + QObject::tr("COMMAND") + "]")
        << CommandHelp("removetab",
                       QObject::tr("Remove tab."))
           .addArg(QObject::tr("NAME"))
        << CommandHelp("renametab",
                       QObject::tr("Rename tab."))
           .addArg(QObject::tr("NAME"))
           .addArg(QObject::tr("NEW_NAME"))
        << CommandHelp()
        << CommandHelp("exporttab",
                       QObject::tr("Export items to file."))
           .addArg(QObject::tr("FILE_NAME"))
        << CommandHelp("importtab",
                       QObject::tr("Import items from file."))
           .addArg(QObject::tr("FILE_NAME"))
        << CommandHelp()
        << CommandHelp("config",
                       QObject::tr("List all options."))
        << CommandHelp("config",
                       QObject::tr("Get option value."))
           .addArg(QObject::tr("OPTION"))
        << CommandHelp("config",
                       QObject::tr("Set option value."))
           .addArg(QObject::tr("OPTION"))
           .addArg(QObject::tr("VALUE"))
        << CommandHelp()
        << CommandHelp("eval, -e",
                       QObject::tr("Evaluate script."))
           .addArg("[" + QObject::tr("SCRIPT") + "]")
        << CommandHelp("help, -h, --help",
                       QObject::tr("\nPrint help for COMMAND or all commands."))
           .addArg("[" + QObject::tr("COMMAND") + "]")
        << CommandHelp("version, -v, --version",
                       QObject::tr("\nPrint version of program and libraries."));
}

QString helpHead()
{
    return QObject::tr("Usage: copyq [%1]").arg(QObject::tr("COMMAND")) + nl
        + nl
        + QObject::tr("Starts server if no command is specified.") + nl
        + QObject::tr("  COMMANDs:");
}

QString helpTail()
{
    return QObject::tr("NOTES:") + nl
        + QObject::tr("  - Use dash argument (-) to read data from stdandard input.") + nl
        + QObject::tr("  - Use double-dash argument (--) to read all following arguments without\n"
                      "    expanding escape sequences (i.e. \\n, \\t and others).") + nl
        + QObject::tr("  - Use ? for MIME to print available MIME types (default is \"text/plain\").") + nl
        + QObject::tr("  - Parameter SCRIPT contains program in ECMAScript, for example:\n"
                      "      copyq -e \"tab('music'); x=''; for(i=0; i<size(); ++i) x+=str(read(i)); x\"\n"
                      "    prints concatenated text of all items in the tab 'music'.\n");
}

QString argumentError()
{
    return QObject::tr("Invalid number or arguments!");
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
        throwError( QObject::tr("Name \"%1\" doesn't refer to a function.").arg(name) );
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
    else
        return m_wnd->createTab(m_currentTab, true);
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
    return tr(programName) + " v" + versionString + " (hluk@email.cz)\n"
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
            (cmd.isNull() ? nl + nl + tr(programName) + " v" + versionString  + " (hluk@email.cz)\n"
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
    QByteArray message = QByteArray::number((qlonglong)m_wnd->showMenu());
    sendMessage(message, CommandActivateWindow);
}

void Scriptable::exit()
{
    // Exit application - respond to client first.
    QByteArray message = QByteArray(tr("Terminating server.\n").toLatin1());
    sendMessage(message, CommandSuccess);
    m_client->flush();
    QApplication::exit(0);
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
#ifdef Q_WS_X11
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
        c->model()->removeRow(row);

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

    for ( int i = 0; i < argumentCount(); ++i ) {
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

    if ( !c->openEditor(text) ) {
        m_wnd->showBrowser(c);
        if ( argumentCount() == 0 || argumentCount() > 1 || row >= c->length() ) {
            c->newItem(text);
            c->edit( c->index(0) );
        } else {
            QModelIndex index = c->index(row);
            c->setCurrent(row);
            c->scrollTo(index, QAbstractItemView::PositionAtTop);
            c->edit(index);
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
    if (argumentCount() != 2) {
        throwError(argumentError());
        return;
    }

    QString mime = toString( argument(0) );
    ClipboardBrowser *c = currentTab();
    QScriptValue value = argument(1);
    QByteArray *bytes = toByteArray(value);
    if (bytes != NULL) {
        QMimeData *data = new QMimeData();
        data->setData(mime, *bytes);
        c->add(data, true);
    } else {
        c->add( toString(value), true );
    }
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
        command.output = true;
        command.input = true;
        command.wait = false;
        command.outputTab = c->getID();
        command.sep = ((i + 1) < argumentCount()) ? toString( argument(i + 1) )
                                                  : QString('\n');
        m_wnd->action(text, command);
    } else {
        QByteArray message = QByteArray::number((qlonglong)m_wnd->openActionDialog(text));
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
                return cm->value(name).toString() + '\n';
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

QScriptValue Scriptable::eval()
{
    const QString script = arg(0);
    return engine()->evaluate(script);
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
    sendMessage(*message, CommandPrint);
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

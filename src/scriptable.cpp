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

const char *const versionString = "1.3.0";

const char *const usageString = "Usage: copyq [COMMAND]\n";

const char *const helpString =
"Starts server if no command is specified.\n"
"  COMMANDs:\n"
"    show    Show main window.\n"
"    hide    Hide main window.\n"
"    toggle  Show or hide main window.\n"
"    menu    Open context menu.\n"
"    exit    Exit server.\n"
"\n"
"    clipboard [MIME]          Print clipboard content.\n"
#ifdef Q_WS_X11
"    selection [MIME]          Print X11 selection content.\n"
#endif
"\n"
"    tab                       List available tab names.\n"
"    tab NAME [COMMAND]        Run command on tab with given name.\n"
"                              Tab is created if it doesn't exist.\n"
"                              Default is the first tab (NAME is empty).\n"
"    removetab NAME            Remove tab.\n"
"    renametab NAME NEW_NAME   Rename tab.\n"
"\n"
"    length, count, size       Print number of items in history.\n"
"    select [ROW=0]            Move item in the row to clipboard.\n"
"    add TEXT...               Add text into clipboard.\n"
"    remove [ROWS=0...]        Remove item in given rows.\n"
"    edit [ROWS...]            Edit clipboard items or edit new one.\n"
"\n"
"    read [MIME|ROW]...        Print raw data of clipboard or item in row.\n"
"    write MIME DATA           Write raw data to clipboard.\n"
"    separator SEPARATOR       Set separator for input items.\n"
"\n"
"    action [ROWS=0...]        Show action dialog.\n"
"    action [ROWS=0...] [PROGRAM [SEPARATOR=\\n]]\n"
"                              Run PROGRAM on item text in the rows.\n"
"                              Use %1 in PROGRAM to pass text as argument.\n"
"\n"
"    exporttab FILE_NAME       Export items to file.\n"
"    importtab FILE_NAME       Import items from file.\n"
"\n"
"    config                    List all options.\n"
"    config OPTION             Get option value.\n"
"    config OPTION VALUE       Set option value.\n"
"\n"
"    eval, -e [SCRIPT]         Evaluate script.\n"
"\n"
"    help, -h, --help [COMMAND]\n"
"      Print help for COMMAND or all commands.\n"
"    version, -v, --version\n"
"      Print version of program and libraries.\n"
"\n"
"NOTES:\n"
"  - Use dash argument (-) to read data from stdandard input.\n"
"  - Use double-dash argument (--) to read all following arguments without\n"
"    expanding escape sequences (i.e. \\n, \\t and others).\n"
"  - Use ? for MIME to print available MIME types (default is \"text/plain\").\n"
"  - Parameter SCRIPT contains program in ECMAScript, for example:\n"
"      copyq -e \"x=''; for(i=0; i<size(); ++i) x+=str(read(i)); x\"\n"
"    prints concatenated text of all items in the first tab.\n";

const QString argumentError = QObject::tr("Invalid number or arguments!");

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
    , m_baClass(NULL)
    , m_currentTab()
    , m_inputSeparator("\n")
{
}

void Scriptable::initEngine(QScriptEngine *eng, const QString &currentPath)
{
    QScriptEngine::QObjectWrapOptions opts =
              QScriptEngine::ExcludeChildObjects
            | QScriptEngine::SkipMethodsInEnumeration
            | QScriptEngine::ExcludeSuperClassMethods
            | QScriptEngine::ExcludeSuperClassProperties
            | QScriptEngine::ExcludeSuperClassContents
            | QScriptEngine::ExcludeDeleteLater;
    QScriptValue obj = eng->newQObject(this, QScriptEngine::QtOwnership, opts);
    eng->setGlobalObject(obj);
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
        context()->throwError(
            QObject::tr("Name \"%1\" doesn't refer to a function.").arg(name) );
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
    // TODO: print help only for functionName
    return tr(usageString, "usage") + '\n'
            + tr(helpString, "command line arguments") + '\n'
            + tr(programName) + " v" + versionString  + " (hluk@email.cz)\n";
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

QScriptValue Scriptable::exit()
{
    // Exit application - respond to client first.
    QTimer *timer = new QTimer(this);
    connect( timer, SIGNAL(timeout()),
             QApplication::instance(), SLOT(quit()) );
    timer->start(0);

    return tr("Terminating server.") + '\n';
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
        context()->throwError( tr("Tab name cannot be empty!") );
    else if ( m_wnd->tabs().indexOf(newName) >= 0 )
        context()->throwError( tr("Tab with given name already exists!") );
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

    bool multiple_edit = (argumentCount() > 1);
    if (multiple_edit) {
        for ( int i = 0; i < argumentCount(); ++i ) {
            value = argument(i);
            if ( toInt(value, row) )
                text.append( c->itemText(row) + getInputSeparator() );
        }
    } else {
        value = argument(0);
        if ( toInt(value, row) )
            text = c->itemText(row);
    }

    if ( !c->openEditor(text) ) {
        m_wnd->showBrowser(c);
        if ( multiple_edit || row >= c->length() ) {
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
        context()->throwError(argumentError);
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
        m_wnd->openActionDialog(text);
    }
}

void Scriptable::exporttab()
{
    const QString &fileName = arg(0);
    ClipboardBrowser *c = currentTab();
    if ( fileName.isNull() ) {
        context()->throwError(argumentError);
    } else if ( !m_wnd->saveTab(getFileName(fileName), m_wnd->tabIndex(c)) ) {
        context()->throwError(
            tr("Cannot save to file \"%1\"!\n").arg(fileName) );
    }
}

void Scriptable::importtab()
{
    const QString &fileName = arg(0);
    if ( fileName.isNull() ) {
        context()->throwError(argumentError);
    } else if ( !m_wnd->loadTab(getFileName(fileName)) ) {
        context()->throwError(
            tr("Cannot import file \"%1\"!\n").arg(fileName) );
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
            opts.append( option + "\n  " + cm->optionToolTip(option) + '\n' );
        return opts;
    } else {
        if ( cm->options().contains(name) ) {
            if ( value.isNull() ) {
                // print option value
                return cm->value(name).toString() + '\n';
            } else {
                // set option
                if ( cm->isVisible() ) {
                    context()->throwError(
                        tr("To modify options from command line you must first "
                           "close the CopyQ Configuration dialog!") );
                } else {
                    cm->setValue(name, value);
                    cm->saveSettings();
                }
            }
        } else {
            context()->throwError( tr("Invalid option!") );
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

int Scriptable::getTabIndexOrError(const QString &name)
{
    int i = m_wnd->tabs().indexOf(name);
    if (i == -1)
        context()->throwError( tr("Tab with given name doesn't exist!") );
    return i;
}

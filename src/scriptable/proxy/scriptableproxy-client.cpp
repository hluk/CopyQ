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

#define BROWSER(tabName, call) \
    ClipboardBrowser *c = fetchBrowser(tabName); \
    if (c) \
        (c->call)

#define STR(str) str

#define INVOKE_(function, arguments, functionCallId) \
    static const auto f = FunctionCallSerializer(STR(#function)).withSlotArguments arguments; \
    const auto args = f.argumentList arguments; \
    emit sendMessage(f.serialize(functionCallId, args), CommandFunctionCall)

#define INVOKE_NO_SNIP(function, arguments) \
    using Result = decltype(function arguments); \
    const auto functionCallId = ++m_lastFunctionCallId; \
    INVOKE_(function, arguments, functionCallId); \
    const auto result = waitForFunctionCallFinished(functionCallId); \
    return result.value<Result>(); \

#define INVOKE_NO_SNIP2(FUNCTION, ARGUMENTS) \
    const auto functionCallId = ++m_lastFunctionCallId; \
    INVOKE_(FUNCTION, ARGUMENTS, functionCallId); \
    waitForFunctionCallFinished(functionCallId); \
    return; \

#define INVOKE(FUNCTION, ARGUMENTS) \
    INVOKE_NO_SNIP(FUNCTION, ARGUMENTS)

#define INVOKE2(FUNCTION, ARGUMENTS) \
    INVOKE_NO_SNIP2(FUNCTION, ARGUMENTS)

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
        const int slotIndex = ScriptableProxy::staticMetaObject.indexOfSlot(m_slotName);
        if (slotIndex == -1) {
            log("Failed to find scriptable proxy slot: " + m_slotName, LogError);
            Q_ASSERT(false);
        }
    }

    QByteArray m_slotName;
};

} // namespace

#ifdef HAS_TESTS
class KeyClicker final {};
#endif // HAS_TESTS

ScriptableProxy::ScriptableProxy(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_wnd(mainWindow)
{
    qRegisterMetaTypeStreamOperators<ClipboardMode>("ClipboardMode");
    qRegisterMetaTypeStreamOperators<Command>("Command");
    qRegisterMetaTypeStreamOperators<NamedValueList>("NamedValueList");
    qRegisterMetaTypeStreamOperators<NotificationButtons>("NotificationButtons");
    qRegisterMetaTypeStreamOperators<ScriptablePath>("ScriptablePath");
    qRegisterMetaTypeStreamOperators<QVector<int>>("QVector<int>");
    qRegisterMetaTypeStreamOperators<QVector<Command>>("QVector<Command>");
    qRegisterMetaTypeStreamOperators<QVector<QVariantMap>>("QVector<QVariantMap>");
    qRegisterMetaTypeStreamOperators<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");
}

void ScriptableProxy::callFunction(const QByteArray &serializedFunctionCall)
{
    if (m_shouldBeDeleted)
        return;

    ++m_functionCallStack;
    auto t = new QTimer(this);
    t->setSingleShot(true);
    QObject::connect( t, &QTimer::timeout, this, [=]() {
        const auto result = callFunctionHelper(serializedFunctionCall);
        emit sendMessage(result, CommandFunctionCallReturnValue);
        t->deleteLater();

        --m_functionCallStack;
        if (m_shouldBeDeleted && m_functionCallStack == 0)
            deleteLater();
    });
    t->start(0);
}

QByteArray ScriptableProxy::callFunctionHelper(const QByteArray &serializedFunctionCall)
{
    QVector<QVariant> arguments;
    QByteArray slotName;
    int functionCallId;
    {
        QDataStream stream(serializedFunctionCall);
        stream.setVersion(QDataStream::Qt_5_0);

        quint32 magicNumber;
        quint32 version;
        stream >> magicNumber >> version;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call preamble", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        if (magicNumber != serializedFunctionCallMagicNumber) {
            log("Unexpected scriptable proxy slot call preamble magic number", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        if (version != serializedFunctionCallVersion) {
            log("Unexpected scriptable proxy slot call preamble version", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> functionCallId;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call ID", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> slotName;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call name", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> arguments;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    const auto slotIndex = metaObject()->indexOfSlot(slotName);
    if (slotIndex == -1) {
        log("Failed to find scriptable proxy slot: " + slotName, LogError);
        Q_ASSERT(false);
        return QByteArray();
    }

    const auto metaMethod = metaObject()->method(slotIndex);
    const auto typeId = metaMethod.returnType();

    QGenericArgument args[9];
    for (int i = 0; i < arguments.size(); ++i) {
        auto &value = arguments[i];
        const int argumentTypeId = metaMethod.parameterType(i);
        if (argumentTypeId == QMetaType::QVariant) {
            args[i] = Q_ARG(QVariant, value);
        } else if ( value.userType() == argumentTypeId ) {
            args[i] = QGenericArgument( value.typeName(), static_cast<void*>(value.data()) );
        } else {
            log( QString("Bad argument type (at index %1) for scriptable proxy slot: %2")
                 .arg(i)
                 .arg(metaMethod.methodSignature().constData()), LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    QVariant returnValue;
    bool called;

    if (typeId == QMetaType::Void) {
        called = metaMethod.invoke(
                this, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
    } else {
        returnValue = QVariant(typeId, nullptr);
        const auto genericReturnValue = returnValue.isValid()
                ? QGenericReturnArgument(returnValue.typeName(), static_cast<void*>(returnValue.data()) )
                : Q_RETURN_ARG(QVariant, returnValue);

        called = metaMethod.invoke(
                this, genericReturnValue,
                args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
    }

    if (!called) {
        log( QString("Bad scriptable proxy slot call: %1")
             .arg(metaMethod.methodSignature().constData()), LogError);
        Q_ASSERT(false);
    }

    QByteArray bytes;
    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream << functionCallId << returnValue;
    }

    return bytes;
}

void ScriptableProxy::setFunctionCallReturnValue(const QByteArray &bytes)
{
    QDataStream stream(bytes);
    int functionCallId;
    QVariant returnValue;
    stream >> functionCallId >> returnValue;
    if (stream.status() != QDataStream::Ok) {
        log("Failed to read scriptable proxy slot call return value", LogError);
        Q_ASSERT(false);
        return;
    }
    emit functionCallFinished(functionCallId, returnValue);
}

void ScriptableProxy::setInputDialogResult(const QByteArray &bytes)
{
    QDataStream stream(bytes);
    int dialogId;
    NamedValueList result;
    stream >> dialogId >> result;
    if (stream.status() != QDataStream::Ok) {
        log("Failed to read input dialog result", LogError);
        Q_ASSERT(false);
        return;
    }
    emit inputDialogFinished(dialogId, result);
}

void ScriptableProxy::safeDeleteLater()
{
    m_shouldBeDeleted = true;
    if (m_functionCallStack == 0)
        deleteLater();
}

QVariantMap ScriptableProxy::getActionData(int id)
{
    INVOKE_NO_SNIP(getActionData, (id));
}

void ScriptableProxy::setActionData(int id, const QVariantMap &data)
{
    INVOKE_NO_SNIP2(setActionData, (id, data));
}

void ScriptableProxy::exit()
{
    INVOKE2(exit, ());
}

void ScriptableProxy::close()
{
    INVOKE2(close, ());
}

bool ScriptableProxy::showWindow()
{
    INVOKE(showWindow, ());
}

bool ScriptableProxy::showWindowAt(QRect rect)
{
    INVOKE(showWindowAt, (rect));
}

bool ScriptableProxy::pasteToCurrentWindow()
{
    INVOKE(pasteToCurrentWindow, ());
}

bool ScriptableProxy::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow, ());
}

bool ScriptableProxy::isMonitoringEnabled()
{
    INVOKE_NO_SNIP(isMonitoringEnabled, ());
}

bool ScriptableProxy::isMainWindowVisible()
{
    INVOKE_NO_SNIP(isMainWindowVisible, ());
}

bool ScriptableProxy::isMainWindowFocused()
{
    INVOKE_NO_SNIP(isMainWindowFocused, ());
}

void ScriptableProxy::disableMonitoring(bool arg1)
{
    INVOKE2(disableMonitoring, (arg1));
}

void ScriptableProxy::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(setClipboard, (data, mode));
}

QString ScriptableProxy::renameTab(const QString &arg1, const QString &arg2)
{
    INVOKE(renameTab, (arg1, arg2));
}

QString ScriptableProxy::removeTab(const QString &arg1)
{
    INVOKE(removeTab, (arg1));
}

QString ScriptableProxy::tabIcon(const QString &tabName)
{
    INVOKE_NO_SNIP(tabIcon, (tabName));
}

void ScriptableProxy::setTabIcon(const QString &tabName, const QString &icon)
{
    INVOKE2(setTabIcon, (tabName, icon));
}

QStringList ScriptableProxy::unloadTabs(const QStringList &tabs)
{
    INVOKE(unloadTabs, (tabs));
}

void ScriptableProxy::forceUnloadTabs(const QStringList &tabs)
{
    INVOKE2(forceUnloadTabs, (tabs));
}

bool ScriptableProxy::showBrowser(const QString &tabName)
{
    INVOKE(showBrowser, (tabName));
}

bool ScriptableProxy::showBrowserAt(const QString &tabName, QRect rect)
{
    INVOKE(showBrowserAt, (tabName, rect));
}

void ScriptableProxy::action(const QVariantMap &arg1, const Command &arg2)
{
    INVOKE2(action, (arg1, arg2));
}

void ScriptableProxy::runInternalAction(const QVariantMap &data, const QString &command)
{
    INVOKE_NO_SNIP2(runInternalAction, (data, command));
}

QByteArray ScriptableProxy::tryGetCommandOutput(const QString &command)
{
    INVOKE_NO_SNIP(tryGetCommandOutput, (command));
}

void ScriptableProxy::showMessage(const QString &title,
        const QString &msg,
        const QString &icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    INVOKE2(showMessage, (title, msg, icon, msec, notificationId, buttons));
}

QVariantMap ScriptableProxy::nextItem(const QString &tabName, int where)
{
    INVOKE(nextItem, (tabName, where));
}

void ScriptableProxy::browserMoveToClipboard(const QString &tabName, int row)
{
    INVOKE2(browserMoveToClipboard, (tabName, row));
}

void ScriptableProxy::browserSetCurrent(const QString &tabName, int arg1)
{
    INVOKE2(browserSetCurrent, (tabName, arg1));
}

QString ScriptableProxy::browserRemoveRows(const QString &tabName, QVector<int> rows)
{
    INVOKE(browserRemoveRows, (tabName, rows));
}

void ScriptableProxy::browserMoveSelected(int targetRow)
{
    INVOKE2(browserMoveSelected, (targetRow));
}

void ScriptableProxy::browserEditRow(const QString &tabName, int arg1)
{
    INVOKE2(browserEditRow, (tabName, arg1));
}

void ScriptableProxy::browserEditNew(const QString &tabName, const QString &arg1, bool changeClipboard)
{
    INVOKE2(browserEditNew, (tabName, arg1, changeClipboard));
}

QStringList ScriptableProxy::tabs()
{
    INVOKE(tabs, ());
}

bool ScriptableProxy::toggleVisible()
{
    INVOKE(toggleVisible, ());
}

bool ScriptableProxy::toggleMenu(const QString &tabName, int maxItemCount, QPoint position)
{
    INVOKE(toggleMenu, (tabName, maxItemCount, position));
}

bool ScriptableProxy::toggleCurrentMenu()
{
    INVOKE(toggleCurrentMenu, ());
}

int ScriptableProxy::findTabIndex(const QString &arg1)
{
    INVOKE(findTabIndex, (arg1));
}

int ScriptableProxy::menuItems(const QVector<QVariantMap> &items)
{
    INVOKE(menuItems, (items));
}

void ScriptableProxy::openActionDialog(const QVariantMap &arg1)
{
    INVOKE2(openActionDialog, (arg1));
}

bool ScriptableProxy::loadTab(const QString &arg1)
{
    INVOKE(loadTab, (arg1));
}

bool ScriptableProxy::saveTab(const QString &tabName, const QString &arg1)
{
    INVOKE(saveTab, (tabName, arg1));
}

bool ScriptableProxy::importData(const QString &fileName)
{
    INVOKE(importData, (fileName));
}

bool ScriptableProxy::exportData(const QString &fileName)
{
    INVOKE(exportData, (fileName));
}

QVariant ScriptableProxy::config(const QStringList &nameValue)
{
    INVOKE(config, (nameValue));
}

QVariant ScriptableProxy::toggleConfig(const QString &optionName)
{
    INVOKE(toggleConfig, (optionName));
}

int ScriptableProxy::browserLength(const QString &tabName)
{
    INVOKE_NO_SNIP(browserLength, (tabName));
}

bool ScriptableProxy::browserOpenEditor(const QString &tabName, const QByteArray &arg1, bool changeClipboard)
{
    INVOKE(browserOpenEditor, (tabName, arg1, changeClipboard));
}

QString ScriptableProxy::browserInsert(const QString &tabName, int row, const QVector<QVariantMap> &items)
{
    INVOKE(browserInsert, (tabName, row, items));
}

bool ScriptableProxy::browserChange(const QString &tabName, const QVariantMap &data, int row)
{
    INVOKE(browserChange, (tabName, data, row));
}

QByteArray ScriptableProxy::browserItemData(const QString &tabName, int arg1, const QString &arg2)
{
    INVOKE(browserItemData, (tabName, arg1, arg2));
}

QVariantMap ScriptableProxy::browserItemData(const QString &tabName, int arg1)
{
    INVOKE(browserItemData, (tabName, arg1));
}

void ScriptableProxy::setCurrentTab(const QString &tabName)
{
    INVOKE2(setCurrentTab, (tabName));
}

QString ScriptableProxy::tab(const QString &tabName)
{
    INVOKE(tab, (tabName));
}

int ScriptableProxy::currentItem()
{
    INVOKE_NO_SNIP(currentItem, ());
}

bool ScriptableProxy::selectItems(const QString &tabName, const QVector<int> &rows)
{
    INVOKE(selectItems, (tabName, rows));
}

QVector<int> ScriptableProxy::selectedItems()
{
    INVOKE_NO_SNIP(selectedItems, ());
}

QVariantMap ScriptableProxy::selectedItemData(int selectedIndex)
{
    INVOKE(selectedItemData, (selectedIndex));
}

bool ScriptableProxy::setSelectedItemData(int selectedIndex, const QVariantMap &data)
{
    INVOKE(setSelectedItemData, (selectedIndex, data));
}

QVector<QVariantMap> ScriptableProxy::selectedItemsData()
{
    INVOKE(selectedItemsData, ());
}

void ScriptableProxy::setSelectedItemsData(const QVector<QVariantMap> &dataList)
{
    INVOKE2(setSelectedItemsData, (dataList));
}

#ifdef HAS_TESTS
void ScriptableProxy::sendKeys(const QString &expectedWidgetName, const QString &keys, int delay)
{
    INVOKE2(sendKeys, (expectedWidgetName, keys, delay));
}

bool ScriptableProxy::sendKeysSucceeded()
{
    INVOKE(sendKeysSucceeded, ());
}

bool ScriptableProxy::sendKeysFailed()
{
    INVOKE(sendKeysFailed, ());
}

QString ScriptableProxy::testSelected()
{
    INVOKE(testSelected, ());
}
#endif // HAS_TESTS

void ScriptableProxy::serverLog(const QString &text)
{
    INVOKE2(serverLog, (text));
}

QString ScriptableProxy::currentWindowTitle()
{
    INVOKE(currentWindowTitle, ());
}

int ScriptableProxy::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog, (values));
}

void ScriptableProxy::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    INVOKE_NO_SNIP2(setSelectedItemsData, (mime, value));
}

void ScriptableProxy::filter(const QString &text)
{
    INVOKE2(filter, (text));
}

QString ScriptableProxy::filter()
{
    INVOKE(filter, ());
}

QVector<Command> ScriptableProxy::commands()
{
    INVOKE_NO_SNIP(commands, ());
}

void ScriptableProxy::setCommands(const QVector<Command> &commands)
{
    INVOKE2(setCommands, (commands));
}

void ScriptableProxy::addCommands(const QVector<Command> &commands)
{
    INVOKE2(addCommands, (commands));
}

QByteArray ScriptableProxy::screenshot(const QString &format, const QString &screenName, bool select)
{
    INVOKE(screenshot, (format, screenName, select));
}

QStringList ScriptableProxy::screenNames()
{
    INVOKE(screenNames, ());
}

Qt::KeyboardModifiers ScriptableProxy::queryKeyboardModifiers()
{
    INVOKE(queryKeyboardModifiers, ());
}

QPoint ScriptableProxy::pointerPosition()
{
    INVOKE(pointerPosition, ());
}

void ScriptableProxy::setPointerPosition(int x, int y)
{
    INVOKE2(setPointerPosition, (x, y));
}

QString ScriptableProxy::pluginsPath()
{
    INVOKE_NO_SNIP(pluginsPath, ());
}

QString ScriptableProxy::themesPath()
{
    INVOKE_NO_SNIP(themesPath, ());
}

QString ScriptableProxy::translationsPath()
{
    INVOKE_NO_SNIP(translationsPath, ());
}

QString ScriptableProxy::iconColor()
{
    INVOKE_NO_SNIP(iconColor, ());
}

bool ScriptableProxy::setIconColor(const QString &colorName)
{
    INVOKE(setIconColor, (colorName));
}

QString ScriptableProxy::iconTag()
{
    INVOKE_NO_SNIP(iconTag, ());
}

void ScriptableProxy::setIconTag(const QString &tag)
{
    INVOKE2(setIconTag, (tag));
}

QString ScriptableProxy::iconTagColor()
{
    INVOKE(iconTagColor, ());
}

bool ScriptableProxy::setIconTagColor(const QString &colorName)
{
    INVOKE(setIconTagColor, (colorName));
}

void ScriptableProxy::setClipboardData(const QVariantMap &data)
{
    INVOKE2(setClipboardData, (data));
}

void ScriptableProxy::setTitle(const QString &title)
{
    INVOKE2(setTitle, (title));
}

void ScriptableProxy::setTitleForData(const QVariantMap &data)
{
    INVOKE2(setTitleForData, (data));
}

void ScriptableProxy::saveData(const QString &tab, const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(saveData, (tab, data, mode));
}

void ScriptableProxy::showDataNotification(const QVariantMap &data)
{
    INVOKE2(showDataNotification, (data));
}

bool ScriptableProxy::enableMenuItem(int actionId, int currentRun, int menuItemMatchCommandIndex, bool enabled)
{
    INVOKE_NO_SNIP(enableMenuItem, (actionId, currentRun, menuItemMatchCommandIndex, enabled));
}

QVariantMap ScriptableProxy::setDisplayData(int actionId, const QVariantMap &displayData)
{
    INVOKE_NO_SNIP(setDisplayData, (actionId, displayData));
}

QVector<Command> ScriptableProxy::automaticCommands()
{
    INVOKE_NO_SNIP(automaticCommands, ());
}

QVector<Command> ScriptableProxy::displayCommands()
{
    INVOKE_NO_SNIP(displayCommands, ());
}

QVector<Command> ScriptableProxy::scriptCommands()
{
    INVOKE_NO_SNIP(scriptCommands, ());
}

bool ScriptableProxy::openUrls(const QStringList &urls)
{
    INVOKE(openUrls, (urls));
}

QString ScriptableProxy::loadTheme(const QString &path)
{
    INVOKE(loadTheme, (path));
}

ClipboardBrowser *ScriptableProxy::fetchBrowser(const QString &)
{
    return nullptr;
}

QVariantMap ScriptableProxy::itemData(const QString &, int)
{
    return {};
}

QByteArray ScriptableProxy::itemData(const QString &, int, const QString &)
{
    return {};
}

ClipboardBrowser *ScriptableProxy::currentBrowser() const
{
    return nullptr;
}

QList<QPersistentModelIndex> ScriptableProxy::selectedIndexes() const
{
    return {};
}

QVariant ScriptableProxy::waitForFunctionCallFinished(int functionCallId)
{
    QVariant result;

    QEventLoop loop;
    connect(this, &ScriptableProxy::functionCallFinished, &loop,
            [&](int receivedFunctionCallId, const QVariant &returnValue) {
                if (receivedFunctionCallId != functionCallId)
                    return;
                result = returnValue;
                loop.quit();
            });
    connect(this, &ScriptableProxy::clientDisconnected, &loop, &QEventLoop::quit);
    connect(qApp, &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

#ifdef HAS_TESTS
KeyClicker *ScriptableProxy::keyClicker()
{
    return nullptr;
}
#endif // HAS_TESTS

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

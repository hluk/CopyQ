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
#include "scriptableproxy-common.h"

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

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

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
        const int slotIndex = ScriptableProxyClient::staticMetaObject.indexOfSlot(m_slotName);
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

ScriptableProxyClient::ScriptableProxyClient(QObject *parent)
    : QObject(parent)
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

void ScriptableProxyClient::callFunction(const QByteArray &serializedFunctionCall)
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

QByteArray ScriptableProxyClient::callFunctionHelper(const QByteArray &serializedFunctionCall)
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

void ScriptableProxyClient::setFunctionCallReturnValue(const QByteArray &bytes)
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

void ScriptableProxyClient::setInputDialogResult(const QByteArray &bytes)
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

void ScriptableProxyClient::safeDeleteLater()
{
    m_shouldBeDeleted = true;
    if (m_functionCallStack == 0)
        deleteLater();
}

QVariantMap ScriptableProxyClient::getActionData(int id)
{
    INVOKE_NO_SNIP(getActionData, (id));
}

void ScriptableProxyClient::setActionData(int id, const QVariantMap &data)
{
    INVOKE_NO_SNIP2(setActionData, (id, data));
}

void ScriptableProxyClient::exit()
{
    INVOKE2(exit, ());
}

void ScriptableProxyClient::close()
{
    INVOKE2(close, ());
}

bool ScriptableProxyClient::showWindow()
{
    INVOKE(showWindow, ());
}

bool ScriptableProxyClient::showWindowAt(QRect rect)
{
    INVOKE(showWindowAt, (rect));
}

bool ScriptableProxyClient::pasteToCurrentWindow()
{
    INVOKE(pasteToCurrentWindow, ());
}

bool ScriptableProxyClient::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow, ());
}

bool ScriptableProxyClient::isMonitoringEnabled()
{
    INVOKE_NO_SNIP(isMonitoringEnabled, ());
}

bool ScriptableProxyClient::isMainWindowVisible()
{
    INVOKE_NO_SNIP(isMainWindowVisible, ());
}

bool ScriptableProxyClient::isMainWindowFocused()
{
    INVOKE_NO_SNIP(isMainWindowFocused, ());
}

void ScriptableProxyClient::disableMonitoring(bool arg1)
{
    INVOKE2(disableMonitoring, (arg1));
}

void ScriptableProxyClient::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(setClipboard, (data, mode));
}

QString ScriptableProxyClient::renameTab(const QString &arg1, const QString &arg2)
{
    INVOKE(renameTab, (arg1, arg2));
}

QString ScriptableProxyClient::removeTab(const QString &arg1)
{
    INVOKE(removeTab, (arg1));
}

QString ScriptableProxyClient::tabIcon(const QString &tabName)
{
    INVOKE_NO_SNIP(tabIcon, (tabName));
}

void ScriptableProxyClient::setTabIcon(const QString &tabName, const QString &icon)
{
    INVOKE2(setTabIcon, (tabName, icon));
}

QStringList ScriptableProxyClient::unloadTabs(const QStringList &tabs)
{
    INVOKE(unloadTabs, (tabs));
}

void ScriptableProxyClient::forceUnloadTabs(const QStringList &tabs)
{
    INVOKE2(forceUnloadTabs, (tabs));
}

bool ScriptableProxyClient::showBrowser(const QString &tabName)
{
    INVOKE(showBrowser, (tabName));
}

bool ScriptableProxyClient::showBrowserAt(const QString &tabName, QRect rect)
{
    INVOKE(showBrowserAt, (tabName, rect));
}

void ScriptableProxyClient::action(const QVariantMap &arg1, const Command &arg2)
{
    INVOKE2(action, (arg1, arg2));
}

void ScriptableProxyClient::runInternalAction(const QVariantMap &data, const QString &command)
{
    INVOKE_NO_SNIP2(runInternalAction, (data, command));
}

QByteArray ScriptableProxyClient::tryGetCommandOutput(const QString &command)
{
    INVOKE_NO_SNIP(tryGetCommandOutput, (command));
}

void ScriptableProxyClient::showMessage(const QString &title,
        const QString &msg,
        const QString &icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    INVOKE2(showMessage, (title, msg, icon, msec, notificationId, buttons));
}

QVariantMap ScriptableProxyClient::nextItem(const QString &tabName, int where)
{
    INVOKE(nextItem, (tabName, where));
}

void ScriptableProxyClient::browserMoveToClipboard(const QString &tabName, int row)
{
    INVOKE2(browserMoveToClipboard, (tabName, row));
}

void ScriptableProxyClient::browserSetCurrent(const QString &tabName, int arg1)
{
    INVOKE2(browserSetCurrent, (tabName, arg1));
}

QString ScriptableProxyClient::browserRemoveRows(const QString &tabName, QVector<int> rows)
{
    INVOKE(browserRemoveRows, (tabName, rows));
}

void ScriptableProxyClient::browserMoveSelected(int targetRow)
{
    INVOKE2(browserMoveSelected, (targetRow));
}

void ScriptableProxyClient::browserEditRow(const QString &tabName, int arg1)
{
    INVOKE2(browserEditRow, (tabName, arg1));
}

void ScriptableProxyClient::browserEditNew(const QString &tabName, const QString &arg1, bool changeClipboard)
{
    INVOKE2(browserEditNew, (tabName, arg1, changeClipboard));
}

QStringList ScriptableProxyClient::tabs()
{
    INVOKE(tabs, ());
}

bool ScriptableProxyClient::toggleVisible()
{
    INVOKE(toggleVisible, ());
}

bool ScriptableProxyClient::toggleMenu(const QString &tabName, int maxItemCount, QPoint position)
{
    INVOKE(toggleMenu, (tabName, maxItemCount, position));
}

bool ScriptableProxyClient::toggleCurrentMenu()
{
    INVOKE(toggleCurrentMenu, ());
}

int ScriptableProxyClient::findTabIndex(const QString &arg1)
{
    INVOKE(findTabIndex, (arg1));
}

int ScriptableProxyClient::menuItems(const QVector<QVariantMap> &items)
{
    INVOKE(menuItems, (items));
}

void ScriptableProxyClient::openActionDialog(const QVariantMap &arg1)
{
    INVOKE2(openActionDialog, (arg1));
}

bool ScriptableProxyClient::loadTab(const QString &arg1)
{
    INVOKE(loadTab, (arg1));
}

bool ScriptableProxyClient::saveTab(const QString &tabName, const QString &arg1)
{
    INVOKE(saveTab, (tabName, arg1));
}

bool ScriptableProxyClient::importData(const QString &fileName)
{
    INVOKE(importData, (fileName));
}

bool ScriptableProxyClient::exportData(const QString &fileName)
{
    INVOKE(exportData, (fileName));
}

QVariant ScriptableProxyClient::config(const QStringList &nameValue)
{
    INVOKE(config, (nameValue));
}

QVariant ScriptableProxyClient::toggleConfig(const QString &optionName)
{
    INVOKE(toggleConfig, (optionName));
}

int ScriptableProxyClient::browserLength(const QString &tabName)
{
    INVOKE_NO_SNIP(browserLength, (tabName));
}

bool ScriptableProxyClient::browserOpenEditor(const QString &tabName, const QByteArray &arg1, bool changeClipboard)
{
    INVOKE(browserOpenEditor, (tabName, arg1, changeClipboard));
}

QString ScriptableProxyClient::browserInsert(const QString &tabName, int row, const QVector<QVariantMap> &items)
{
    INVOKE(browserInsert, (tabName, row, items));
}

bool ScriptableProxyClient::browserChange(const QString &tabName, const QVariantMap &data, int row)
{
    INVOKE(browserChange, (tabName, data, row));
}

QByteArray ScriptableProxyClient::browserItemData(const QString &tabName, int arg1, const QString &arg2)
{
    INVOKE(browserItemData, (tabName, arg1, arg2));
}

QVariantMap ScriptableProxyClient::browserItemData(const QString &tabName, int arg1)
{
    INVOKE(browserItemData, (tabName, arg1));
}

void ScriptableProxyClient::setCurrentTab(const QString &tabName)
{
    INVOKE2(setCurrentTab, (tabName));
}

QString ScriptableProxyClient::tab(const QString &tabName)
{
    INVOKE(tab, (tabName));
}

int ScriptableProxyClient::currentItem()
{
    INVOKE_NO_SNIP(currentItem, ());
}

bool ScriptableProxyClient::selectItems(const QString &tabName, const QVector<int> &rows)
{
    INVOKE(selectItems, (tabName, rows));
}

QVector<int> ScriptableProxyClient::selectedItems()
{
    INVOKE_NO_SNIP(selectedItems, ());
}

QVariantMap ScriptableProxyClient::selectedItemData(int selectedIndex)
{
    INVOKE(selectedItemData, (selectedIndex));
}

bool ScriptableProxyClient::setSelectedItemData(int selectedIndex, const QVariantMap &data)
{
    INVOKE(setSelectedItemData, (selectedIndex, data));
}

QVector<QVariantMap> ScriptableProxyClient::selectedItemsData()
{
    INVOKE(selectedItemsData, ());
}

void ScriptableProxyClient::setSelectedItemsData(const QVector<QVariantMap> &dataList)
{
    INVOKE2(setSelectedItemsData, (dataList));
}

#ifdef HAS_TESTS
void ScriptableProxyClient::sendKeys(const QString &expectedWidgetName, const QString &keys, int delay)
{
    INVOKE2(sendKeys, (expectedWidgetName, keys, delay));
}

bool ScriptableProxyClient::sendKeysSucceeded()
{
    INVOKE(sendKeysSucceeded, ());
}

bool ScriptableProxyClient::sendKeysFailed()
{
    INVOKE(sendKeysFailed, ());
}

QString ScriptableProxyClient::testSelected()
{
    INVOKE(testSelected, ());
}
#endif // HAS_TESTS

void ScriptableProxyClient::serverLog(const QString &text)
{
    INVOKE2(serverLog, (text));
}

QString ScriptableProxyClient::currentWindowTitle()
{
    INVOKE(currentWindowTitle, ());
}

int ScriptableProxyClient::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog, (values));
}

void ScriptableProxyClient::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    INVOKE_NO_SNIP2(setSelectedItemsData, (mime, value));
}

void ScriptableProxyClient::filter(const QString &text)
{
    INVOKE2(filter, (text));
}

QString ScriptableProxyClient::filter()
{
    INVOKE(filter, ());
}

QVector<Command> ScriptableProxyClient::commands()
{
    INVOKE_NO_SNIP(commands, ());
}

void ScriptableProxyClient::setCommands(const QVector<Command> &commands)
{
    INVOKE2(setCommands, (commands));
}

void ScriptableProxyClient::addCommands(const QVector<Command> &commands)
{
    INVOKE2(addCommands, (commands));
}

QByteArray ScriptableProxyClient::screenshot(const QString &format, const QString &screenName, bool select)
{
    INVOKE(screenshot, (format, screenName, select));
}

QStringList ScriptableProxyClient::screenNames()
{
    INVOKE(screenNames, ());
}

Qt::KeyboardModifiers ScriptableProxyClient::queryKeyboardModifiers()
{
    INVOKE(queryKeyboardModifiers, ());
}

QPoint ScriptableProxyClient::pointerPosition()
{
    INVOKE(pointerPosition, ());
}

void ScriptableProxyClient::setPointerPosition(int x, int y)
{
    INVOKE2(setPointerPosition, (x, y));
}

QString ScriptableProxyClient::pluginsPath()
{
    INVOKE_NO_SNIP(pluginsPath, ());
}

QString ScriptableProxyClient::themesPath()
{
    INVOKE_NO_SNIP(themesPath, ());
}

QString ScriptableProxyClient::translationsPath()
{
    INVOKE_NO_SNIP(translationsPath, ());
}

QString ScriptableProxyClient::iconColor()
{
    INVOKE_NO_SNIP(iconColor, ());
}

bool ScriptableProxyClient::setIconColor(const QString &colorName)
{
    INVOKE(setIconColor, (colorName));
}

QString ScriptableProxyClient::iconTag()
{
    INVOKE_NO_SNIP(iconTag, ());
}

void ScriptableProxyClient::setIconTag(const QString &tag)
{
    INVOKE2(setIconTag, (tag));
}

QString ScriptableProxyClient::iconTagColor()
{
    INVOKE(iconTagColor, ());
}

bool ScriptableProxyClient::setIconTagColor(const QString &colorName)
{
    INVOKE(setIconTagColor, (colorName));
}

void ScriptableProxyClient::setClipboardData(const QVariantMap &data)
{
    INVOKE2(setClipboardData, (data));
}

void ScriptableProxyClient::setTitle(const QString &title)
{
    INVOKE2(setTitle, (title));
}

void ScriptableProxyClient::setTitleForData(const QVariantMap &data)
{
    INVOKE2(setTitleForData, (data));
}

void ScriptableProxyClient::saveData(const QString &tab, const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(saveData, (tab, data, mode));
}

void ScriptableProxyClient::showDataNotification(const QVariantMap &data)
{
    INVOKE2(showDataNotification, (data));
}

bool ScriptableProxyClient::enableMenuItem(int actionId, int currentRun, int menuItemMatchCommandIndex, bool enabled)
{
    INVOKE_NO_SNIP(enableMenuItem, (actionId, currentRun, menuItemMatchCommandIndex, enabled));
}

QVariantMap ScriptableProxyClient::setDisplayData(int actionId, const QVariantMap &displayData)
{
    INVOKE_NO_SNIP(setDisplayData, (actionId, displayData));
}

QVector<Command> ScriptableProxyClient::automaticCommands()
{
    INVOKE_NO_SNIP(automaticCommands, ());
}

QVector<Command> ScriptableProxyClient::displayCommands()
{
    INVOKE_NO_SNIP(displayCommands, ());
}

QVector<Command> ScriptableProxyClient::scriptCommands()
{
    INVOKE_NO_SNIP(scriptCommands, ());
}

bool ScriptableProxyClient::openUrls(const QStringList &urls)
{
    INVOKE(openUrls, (urls));
}

QString ScriptableProxyClient::loadTheme(const QString &path)
{
    INVOKE(loadTheme, (path));
}

QVariant ScriptableProxyClient::waitForFunctionCallFinished(int functionCallId)
{
    QVariant result;

    QEventLoop loop;
    connect(this, &ScriptableProxyClient::functionCallFinished, &loop,
            [&](int receivedFunctionCallId, const QVariant &returnValue) {
                if (receivedFunctionCallId != functionCallId)
                    return;
                result = returnValue;
                loop.quit();
            });
    connect(this, &ScriptableProxyClient::clientDisconnected, &loop, &QEventLoop::quit);
    connect(qApp, &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

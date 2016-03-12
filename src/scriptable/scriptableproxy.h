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

#ifndef SCRIPTABLEPROXY_H
#define SCRIPTABLEPROXY_H

#include "gui/clipboardbrowser.h"

#include <QClipboard>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QSystemTrayIcon>

class MainWindow;
class QPersistentModelIndex;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

typedef QVector<NamedValue> NamedValueList;

Q_DECLARE_METATYPE(NamedValueList)

#ifdef HAS_TESTS
#   include <QTest>
#endif

#if QT_VERSION < 0x050300
Q_DECLARE_METATYPE(QSystemTrayIcon::MessageIcon)
Q_DECLARE_METATYPE(QList<int>)
#endif

#define BEGIN_INVOKE(methodName) \
    m_helper->unsetValue(); \
    QMetaObject::invokeMethod(m_helper, methodName, Qt::BlockingQueuedConnection

#define END_INVOKE )

#define END_INVOKE_AND_RETURN(RetType) \
    END_INVOKE; \
    const QVariant v = m_helper->value(); \
    Q_ASSERT( !m_helper->isValueUnset() ); \
    Q_ASSERT( qMetaTypeId<RetType>() == qMetaTypeId<QVariant>() || v.userType() == qMetaTypeId<RetType>() ); \
    return v.value<RetType>()

// Compile-time check ScriptableProxyHelper member method arguments.
#define CAN_INVOKE_VOID(methodName) \
    if (false) m_helper->methodName

#define CAN_INVOKE(RetType, call) \
    if (false) static_cast<const RetType &>(m_helper->call)

#define PROXY_METHOD(methodName) \
    void methodName() \
    { \
        CAN_INVOKE_VOID(methodName)(); \
        BEGIN_INVOKE(#methodName) \
        END_INVOKE; \
    }

#define PROXY_METHOD_VOID_1(methodName, Arg1Type) \
    void methodName(Arg1Type arg1) \
    { \
        CAN_INVOKE_VOID(methodName)(arg1); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE; \
    }

#define PROXY_METHOD_VOID_2(methodName, Arg1Type, Arg2Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        CAN_INVOKE_VOID(methodName)(arg1, arg2); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE; \
    }

#define PROXY_METHOD_VOID_4(methodName, Arg1Type, Arg2Type, Arg3Type, Arg4Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4) \
    { \
        CAN_INVOKE_VOID(methodName)(arg1, arg2, arg3, arg4); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
            , Q_ARG(Arg3Type, arg3) \
            , Q_ARG(Arg4Type, arg4) \
        END_INVOKE; \
    }

#define PROXY_METHOD_0(RetType, methodName) \
    RetType methodName() \
    { \
        CAN_INVOKE(RetType, methodName()); \
        BEGIN_INVOKE(#methodName) \
        END_INVOKE_AND_RETURN(RetType); \
    } \

#define PROXY_METHOD_1(RetType, methodName, Arg1Type) \
    RetType methodName(Arg1Type arg1) \
    { \
        CAN_INVOKE(RetType, methodName(arg1)); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE_AND_RETURN(RetType); \
    } \

#define PROXY_METHOD_2(RetType, methodName, Arg1Type, Arg2Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        CAN_INVOKE(RetType, methodName(arg1, arg2)); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE_AND_RETURN(RetType); \
    } \

#define PROXY_METHOD_3(RetType, methodName, Arg1Type, Arg2Type, Arg3Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3) \
    { \
        CAN_INVOKE(RetType, methodName(arg1, arg2, arg3)); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
            , Q_ARG(Arg3Type, arg3) \
        END_INVOKE_AND_RETURN(RetType); \
    } \

namespace detail {

class ScriptableProxyHelper : public QObject
{
    Q_OBJECT
public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    ScriptableProxyHelper(MainWindow* mainWindow, const QVariantMap &actionData);

    const QVariant &value() const;

    void unsetValue();

    bool isValueUnset();

    static QString tabNotFoundError();

    static QString tabNameEmptyError();

public slots:
    void close();
    void showWindow();
    void showWindowAt(const QRect &rect);
    bool pasteToCurrentWindow();
    bool copyFromCurrentWindow();

    void abortAutomaticCommands();

    bool isMonitoringEnabled();
    bool isMainWindowVisible();
    bool isMainWindowFocused();
    void disableMonitoring(bool arg1);
    void setClipboard(const QVariantMap &data, QClipboard::Mode mode);

    QString renameTab(const QString &arg1, const QString &arg2);

    QString removeTab(const QString &arg1);

    QString tabIcon(const QString &tabName);
    void setTabIcon(const QString &tabName, const QString &iconName);

    void showBrowser(const QString &tabName);
    void showBrowserAt(const QString &tabName, const QRect &rect);

    void showBrowser();

    void action(const QVariantMap &arg1, const Command &arg2);

    void showMessage(const QString &arg1, const QString &arg2, QSystemTrayIcon::MessageIcon arg3, int arg4);

    void browserLock();

    void browserUnlock();

    QVariantMap nextItem(int where);
    void browserMoveToClipboard(int arg1);
    void browserSetCurrent(int arg1);
    void browserRemoveRows(QList<int> rows);

    void browserEditRow(int arg1);
    void browserEditNew(const QString &arg1, bool changeClipboard);

    QStringList tabs();
    bool toggleVisible();
    bool toggleMenu(const QString &tabName);
    bool toggleMenu();
    QByteArray mainWinId();
    QByteArray trayMenuWinId();
    int findTabIndex(const QString &arg1);

    QByteArray openActionDialog(const QVariantMap &arg1);

    bool loadTab(const QString &arg1);
    bool saveTab(const QString &arg1);

    QVariant config(const QString &name, const QString &value);

    QByteArray getClipboardData(const QString &mime, QClipboard::Mode mode = QClipboard::Clipboard);

    int browserLength();
    bool browserOpenEditor(const QByteArray &arg1, bool changeClipboard);

    bool browserAdd(const QString &arg1);
    bool browserAdd(const QStringList &texts);
    bool browserAdd(const QVariantMap &arg1, int arg2);
    bool browserChange(const QVariantMap &data, int row);

    QByteArray browserItemData(int arg1, const QString &arg2);
    QVariantMap browserItemData(int arg1);

    void setCurrentTab(const QString &tabName);

    void setTab(const QString &tabName);

    QString tab();

    int currentItem();
    bool selectItems(const QList<int> &items);

    QList<int> selectedItems();

    QString sendKeys(const QString &keys);
    QString testSelected();

    void keyClick(const QKeySequence &shortcut, const QPointer<QWidget> &widget);

    QString currentWindowTitle();

    NamedValueList inputDialog(const NamedValueList &values);

    void setUserValue(const QString &key, const QVariant &value);

    void updateFirstItem(const QVariantMap &data);
    void updateTitle(const QVariantMap &data);
    void setSelectedItemsData(const QString &mime, const QVariant &value);

    void filter(const QString &text);

signals:
    void sendMessage(const QByteArray &message, int messageCode);

private:
    ClipboardBrowser *fetchBrowser(const QString &tabName);
    ClipboardBrowser *fetchBrowser();

    QVariantMap itemData(int i);
    QByteArray itemData(int i, const QString &mime);

    bool canUseSelectedItems() const;
    QList<QPersistentModelIndex> selectedIndexes() const;

    MainWindow* m_wnd;
    QVariant v; ///< Last return value retrieved.
    bool m_valueUnset;
    QString m_tabName;
    QScopedPointer<ClipboardBrowser::Lock> m_lock;
    QVariantMap m_actionData;
};

} // namespace detail

/**
 * Invoke methods (of MainWindow and its ClipboardBrowser objects) from different thread.
 *
 * Methods are invoked using ScriptableProxyHelper that is moved to the same thread as
 * MainWindow object. The main reason for this is that QMetaObject::invokeMethod() cannot return
 * value (expecially in Qt 4.7) when called with Qt::BlockingQueuedConnection.
 */
class ScriptableProxy
{
public:
    ScriptableProxy(MainWindow *mainWindow, const QVariantMap &actionData);

    ~ScriptableProxy();

    QObject *signaler() const { return m_helper; }

    PROXY_METHOD(close)
    PROXY_METHOD(showWindow)
    PROXY_METHOD_VOID_1(showWindowAt, const QRect &)
    PROXY_METHOD_0(bool, pasteToCurrentWindow)
    PROXY_METHOD_0(bool, copyFromCurrentWindow)

    PROXY_METHOD(abortAutomaticCommands)

    PROXY_METHOD_0(bool, isMonitoringEnabled)
    PROXY_METHOD_0(bool, isMainWindowVisible)
    PROXY_METHOD_0(bool, isMainWindowFocused)
    PROXY_METHOD_VOID_1(disableMonitoring, bool)
    PROXY_METHOD_VOID_2(setClipboard, const QVariantMap &, QClipboard::Mode)

    PROXY_METHOD_2(QString, renameTab, const QString &, const QString &)
    PROXY_METHOD_1(QString, removeTab, const QString &)

    PROXY_METHOD_1(QString, tabIcon, const QString &)
    PROXY_METHOD_VOID_2(setTabIcon, const QString &, const QString &)

    PROXY_METHOD_0(QStringList, tabs)
    PROXY_METHOD_0(bool, toggleVisible)
    PROXY_METHOD_0(bool, toggleMenu)
    PROXY_METHOD_1(bool, toggleMenu, const QString &)
    PROXY_METHOD_0(QByteArray, mainWinId)
    PROXY_METHOD_0(QByteArray, trayMenuWinId)
    PROXY_METHOD_1(int, findTabIndex, const QString &)

    PROXY_METHOD(showBrowser)
    PROXY_METHOD_VOID_1(showBrowser, const QString &)
    PROXY_METHOD_VOID_2(showBrowserAt, const QString &, const QRect &)

    PROXY_METHOD_1(QByteArray, openActionDialog, const QVariantMap &)
    PROXY_METHOD_VOID_2(action, const QVariantMap &, const Command &)

    PROXY_METHOD_1(bool, loadTab, const QString &)
    PROXY_METHOD_1(bool, saveTab, const QString &)

    PROXY_METHOD_2(QVariant, config, const QString &, const QString &)

    PROXY_METHOD_VOID_4(showMessage, const QString &, const QString &,
                        QSystemTrayIcon::MessageIcon, int)

    PROXY_METHOD_1(QByteArray, getClipboardData, const QString &)
    PROXY_METHOD_2(QByteArray, getClipboardData, const QString &, QClipboard::Mode)

    PROXY_METHOD(browserLock)
    PROXY_METHOD(browserUnlock)
    PROXY_METHOD_1(QVariantMap, nextItem, int)
    PROXY_METHOD_VOID_1(browserMoveToClipboard, int)
    PROXY_METHOD_VOID_1(browserRemoveRows, const QList<int> &)
    PROXY_METHOD_VOID_1(browserSetCurrent, int)
    PROXY_METHOD_0(int, browserLength)
    PROXY_METHOD_2(bool, browserOpenEditor, const QByteArray &, bool)

    PROXY_METHOD_1(bool, browserAdd, const QString &)
    PROXY_METHOD_1(bool, browserAdd, const QStringList &)
    PROXY_METHOD_2(bool, browserAdd, const QVariantMap &, int)
    PROXY_METHOD_2(bool, browserChange, const QVariantMap &, int)
    PROXY_METHOD_VOID_1(browserEditRow, int)
    PROXY_METHOD_VOID_2(browserEditNew, const QString &, bool)

    PROXY_METHOD_2(QByteArray, browserItemData, int, const QString &)
    PROXY_METHOD_1(QVariantMap, browserItemData, int)

    PROXY_METHOD_VOID_1(setCurrentTab, const QString &)

    PROXY_METHOD_VOID_1(setTab, const QString &)
    PROXY_METHOD_0(QString, tab)

    PROXY_METHOD_0(int, currentItem)
    PROXY_METHOD_1(bool, selectItems, const QList<int> &)
    PROXY_METHOD_0(QList<int>, selectedItems)

    PROXY_METHOD_1(QString, sendKeys, const QString &)
    PROXY_METHOD_0(QString, testSelected)

    PROXY_METHOD_0(QString, currentWindowTitle)

    PROXY_METHOD_1(NamedValueList, inputDialog, const NamedValueList &)

    PROXY_METHOD_VOID_2(setUserValue, const QString &, const QVariant &)

    PROXY_METHOD_VOID_1(updateFirstItem, const QVariantMap &)
    PROXY_METHOD_VOID_1(updateTitle, const QVariantMap &)
    PROXY_METHOD_VOID_2(setSelectedItemsData, const QString &, const QVariant &)

    PROXY_METHOD_VOID_1(filter, const QString &)

private:
    detail::ScriptableProxyHelper *m_helper; ///< For retrieving return values of methods in MainWindow.
};

#endif // SCRIPTABLEPROXY_H

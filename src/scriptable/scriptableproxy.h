/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifdef HAS_TESTS
#   include <QTest>
#endif

#if QT_VERSION < 0x050300
Q_DECLARE_METATYPE(QSystemTrayIcon::MessageIcon)
Q_DECLARE_METATYPE(QList<int>)
#endif

#define BEGIN_INVOKE(methodName) \
        QMetaObject::invokeMethod(m_helper, methodName, Qt::BlockingQueuedConnection \

#define END_INVOKE );

#define END_INVOKE_AND_RETURN(RetType) END_INVOKE return m_helper->value().value<RetType>();

// Compile-time check ScriptableProxyHelper member method arguments.
#define CAN_INVOKE(methodName) \
    if (false) m_helper->methodName

#define PROXY_METHOD(methodName) \
    void methodName() \
    { \
        CAN_INVOKE(methodName)(); \
        BEGIN_INVOKE(#methodName) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_1(methodName, Arg1Type) \
    void methodName(Arg1Type arg1) \
    { \
        CAN_INVOKE(methodName)(arg1); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_2(methodName, Arg1Type, Arg2Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        CAN_INVOKE(methodName)(arg1, arg2); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_4(methodName, Arg1Type, Arg2Type, Arg3Type, Arg4Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4) \
    { \
        CAN_INVOKE(methodName)(arg1, arg2, arg3, arg4); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
            , Q_ARG(Arg3Type, arg3) \
            , Q_ARG(Arg4Type, arg4) \
        END_INVOKE \
    }

#define PROXY_METHOD_0(RetType, methodName) \
    RetType methodName() \
    { \
        CAN_INVOKE(methodName)(); \
        BEGIN_INVOKE(#methodName) \
        END_INVOKE_AND_RETURN(RetType) \
    } \

#define PROXY_METHOD_1(RetType, methodName, Arg1Type) \
    RetType methodName(Arg1Type arg1) \
    { \
        CAN_INVOKE(methodName)(arg1); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE_AND_RETURN(RetType) \
    } \

#define PROXY_METHOD_2(RetType, methodName, Arg1Type, Arg2Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        CAN_INVOKE(methodName)(arg1, arg2); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE_AND_RETURN(RetType) \
    } \

#define PROXY_METHOD_3(RetType, methodName, Arg1Type, Arg2Type, Arg3Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3) \
    { \
        CAN_INVOKE(methodName)(arg1, arg2, arg3); \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
            , Q_ARG(Arg3Type, arg3) \
        END_INVOKE_AND_RETURN(RetType) \
    } \

namespace detail {

class ScriptableProxyHelper : public QObject
{
    Q_OBJECT
public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    explicit ScriptableProxyHelper(MainWindow* mainWindow);

    const QVariant &value() const;

    static QString tabNotFoundError();

    static QString tabNameEmptyError();

public slots:
    void close();
    void showWindow();
    void pasteToCurrentWindow();

    void ignoreCurrentClipboard();

    void isMonitoringEnabled();
    void disableMonitoring(bool arg1);
    void setClipboard(const QVariantMap &arg1);

    void renameTab(const QString &arg1, const QString &arg2);

    void removeTab(const QString &arg1);

    void showBrowser(const QString &tabName);

    void showBrowser();

    void action(const QVariantMap &arg1, const Command &arg2);

    void showMessage(const QString &arg1, const QString &arg2, QSystemTrayIcon::MessageIcon arg3, int arg4);

    void browserLock();

    void browserUnlock();

    void browserCopyNextItemToClipboard();
    void browserCopyPreviousItemToClipboard();
    void browserMoveToClipboard(int arg1);
    void browserSetCurrent(int arg1);
    void browserRemoveRows(QList<int> rows);

    void browserEditRow(int arg1);
    void browserEditNew(const QString &arg1);

    void tabs();
    void toggleVisible();
    void toggleMenu(const QString &tabName);
    void toggleMenu();
    void mainWinId();
    void trayMenuWinId();
    void findTabIndex(const QString &arg1);

    void openActionDialog(const QVariantMap &arg1);

    void loadTab(const QString &arg1);
    void saveTab(const QString &arg1);

    void config(const QString &arg1, const QString &arg2);

    void getClipboardData(const QString &arg1);
    void getClipboardData(const QString &arg1, QClipboard::Mode arg2);

    void getActionData(const QByteArray &arg1, const QString &arg2);

    void browserLength();
    void browserOpenEditor(const QByteArray &arg1);

    void browserAdd(const QString &arg1);
    void browserAdd(const QVariantMap &arg1, int arg2);
    void browserAdd(const QStringList &texts);

    void browserItemData(int arg1, const QString &arg2);
    void browserItemData(int arg1);

    void setCurrentTab(const QString &tabName);

    void currentTab();

    void currentItem();
    void selectItems(const QList<int> &items);

    void selectedTab();
    void selectedItems();
    void index();

    void sendKeys(const QString &keys);

    void keyClick(const QKeySequence &shortcut, const QPointer<QWidget> &widget);

private:
    ClipboardBrowser *fetchBrowser(const QString &tabName);
    ClipboardBrowser *fetchBrowser();

    MainWindow* m_wnd;
    QVariant v; ///< Last return value retrieved.
    QString m_tabName;
    QScopedPointer<ClipboardBrowser::Lock> m_lock;
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
    explicit ScriptableProxy(MainWindow *mainWindow);

    ~ScriptableProxy();

    PROXY_METHOD(close)
    PROXY_METHOD(showWindow)
    PROXY_METHOD(pasteToCurrentWindow)

    PROXY_METHOD(ignoreCurrentClipboard)

    PROXY_METHOD_0(bool, isMonitoringEnabled)
    PROXY_METHOD_VOID_1(disableMonitoring, bool)
    PROXY_METHOD_VOID_1(setClipboard, const QVariantMap &)

    PROXY_METHOD_2(QString, renameTab, const QString &, const QString &)
    PROXY_METHOD_1(QString, removeTab, const QString &)

    PROXY_METHOD_0(QStringList, tabs)
    PROXY_METHOD_0(bool, toggleVisible)
    PROXY_METHOD_0(bool, toggleMenu)
    PROXY_METHOD_1(bool, toggleMenu, const QString &)
    PROXY_METHOD_0(QByteArray, mainWinId)
    PROXY_METHOD_0(QByteArray, trayMenuWinId)
    PROXY_METHOD_1(int, findTabIndex, const QString &)

    PROXY_METHOD(showBrowser)
    PROXY_METHOD_VOID_1(showBrowser, const QString &)

    PROXY_METHOD_1(qulonglong, openActionDialog, const QVariantMap &)
    PROXY_METHOD_VOID_2(action, const QVariantMap &, const Command &)

    PROXY_METHOD_1(bool, loadTab, const QString &)
    PROXY_METHOD_1(bool, saveTab, const QString &)

    PROXY_METHOD_2(QVariant, config, const QString &, const QString &)

    PROXY_METHOD_VOID_4(showMessage, const QString &, const QString &,
                        QSystemTrayIcon::MessageIcon, int)

    PROXY_METHOD_1(QByteArray, getClipboardData, const QString &)
    PROXY_METHOD_2(QByteArray, getClipboardData, const QString &, QClipboard::Mode)

    PROXY_METHOD_2(QByteArray, getActionData, const QByteArray &, const QString &)

    PROXY_METHOD(browserLock)
    PROXY_METHOD(browserUnlock)
    PROXY_METHOD(browserCopyNextItemToClipboard)
    PROXY_METHOD(browserCopyPreviousItemToClipboard)
    PROXY_METHOD_VOID_1(browserMoveToClipboard, int)
    PROXY_METHOD_VOID_1(browserRemoveRows, const QList<int> &)
    PROXY_METHOD_VOID_1(browserSetCurrent, int)
    PROXY_METHOD_0(int, browserLength)
    PROXY_METHOD_1(bool, browserOpenEditor, const QByteArray &)

    PROXY_METHOD_1(bool, browserAdd, const QString &)
    PROXY_METHOD_1(bool, browserAdd, const QStringList &)
    PROXY_METHOD_2(bool, browserAdd, const QVariantMap &, int)
    PROXY_METHOD_VOID_1(browserEditRow, int)
    PROXY_METHOD_VOID_1(browserEditNew, const QString &)

    PROXY_METHOD_2(QByteArray, browserItemData, int, const QString &)
    PROXY_METHOD_1(QVariantMap, browserItemData, int)

    PROXY_METHOD_VOID_1(setCurrentTab, const QString &)
    PROXY_METHOD_0(QString, currentTab)

    PROXY_METHOD_0(int, currentItem)
    PROXY_METHOD_1(bool, selectItems, const QList<int> &)
    PROXY_METHOD_0(QString, selectedTab)
    PROXY_METHOD_0(QList<int>, selectedItems)
    PROXY_METHOD_0(int, index)

    PROXY_METHOD_1(QString, sendKeys, const QString &)

private:
    detail::ScriptableProxyHelper *m_helper; ///< For retrieving return values of methods in MainWindow.
};

#endif // SCRIPTABLEPROXY_H

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

#ifndef SCRIPTABLEPROXY_H
#define SCRIPTABLEPROXY_H

#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/clipboarditem.h"

#include <QMetaObject>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

Q_DECLARE_METATYPE(QSystemTrayIcon::MessageIcon)

#define BEGIN_INVOKE(methodName) \
        QMetaObject::invokeMethod(m_helper, methodName, Qt::BlockingQueuedConnection \

#define END_INVOKE_AND_RETURN(RetType) ); return m_helper->value().value<RetType>();

#define END_INVOKE );

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
    explicit ScriptableProxyHelper(const QSharedPointer<MainWindow> &mainWindow)
      : QObject(NULL)
      , m_wnd(mainWindow)
    {}

    const QVariant &value() const { return v; }

public slots:
    void close() { m_wnd->close(); }
    void showWindow() { m_wnd->showWindow(); }
    void pasteToCurrentWindow() { m_wnd->pasteToCurrentWindow(); }

    void disableMonitoring(bool arg1) { m_wnd->disableClipboardStoring(arg1); }
    void setClipboard(const ClipboardItem *arg1) { m_wnd->setClipboard(arg1); }

    void renameTab(const QString &arg1, int arg2) { m_wnd->renameTab(arg1, arg2); }
    void removeTab(bool arg1, int arg2) { m_wnd->removeTab(arg1, arg2); }

    void createTab(const QString &arg1) { m_wnd->createTab(arg1); }

    void showBrowser(int arg1) { m_wnd->showBrowser(arg1); }

    void action(const QVariantMap &arg1, const Command &arg2) { m_wnd->action(arg1, arg2); }

    void showMessage(const QString &arg1, const QString &arg2, QSystemTrayIcon::MessageIcon arg3, int arg4) { m_wnd->showMessage(arg1, arg2, arg3, arg4); }

    void browserLock(int arg1) { m_wnd->browser(arg1)->lock(); }
    void browserUnlock(int arg1) { m_wnd->browser(arg1)->unlock(); }
    void browserCopyNextItemToClipboard(int arg1) { m_wnd->browser(arg1)->copyNextItemToClipboard(); }
    void browserCopyPreviousItemToClipboard(int arg1) { m_wnd->browser(arg1)->copyPreviousItemToClipboard(); }
    void browserMoveToClipboard(int arg1, int arg2) { m_wnd->browser(arg1)->moveToClipboard(arg2); }
    void browserDelayedSaveItems(int arg1) { m_wnd->browser(arg1)->delayedSaveItems(); }
    void browserRemoveRow(int arg1, int arg2) { m_wnd->browser(arg1)->removeRow(arg2); }
    void browserSetCurrent(int arg1, int arg2) { m_wnd->browser(arg1)->setCurrent(arg2); }

    void browserEditRow(int arg1, int arg2) { m_wnd->browser(arg1)->editRow(arg2); }
    void browserEditNew(int arg1, const QString &arg2) { m_wnd->browser(arg1)->editNew(arg2); }

    void tabs() { v = m_wnd->tabs(); }
    void toggleVisible() { v = m_wnd->toggleVisible(); }
    void toggleMenu() { v = m_wnd->toggleMenu(); }
    void mainWinId() { v = (qulonglong)m_wnd->mainWinId(); }
    void trayMenuWinId() { v = (qulonglong)m_wnd->trayMenuWinId(); }
    void findTabIndex(const QString &arg1) { v = m_wnd->findTabIndex(arg1); }

    void openActionDialog(const QVariantMap &arg1) { v = (qulonglong)m_wnd->openActionDialog(arg1); }

    void loadTab(const QString &arg1) { v = m_wnd->loadTab(arg1); }
    void saveTab(const QString &arg1, int arg2) { v = m_wnd->saveTab(arg1, arg2); }

    void config(const QString &arg1, const QString &arg2) { v = m_wnd->config(arg1, arg2); }

    void getClipboardData(const QString &arg1) { v = m_wnd->getClipboardData(arg1); }
    void getClipboardData(const QString &arg1, QClipboard::Mode arg2) { v = m_wnd->getClipboardData(arg1, arg2); }

    void browserLength(int arg1) { v = m_wnd->browser(arg1)->length(); }
    void browserOpenEditor(int arg1, const QByteArray &arg2) { v = m_wnd->browser(arg1)->openEditor(arg2); }

    void browserAdd(int arg1, const QString &arg2) { v = m_wnd->browser(arg1)->add(arg2); }
    void browserAdd(int arg1, const QVariantMap &arg2, int arg3) { v = m_wnd->browser(arg1)->add(arg2, arg3); }

    void browserItemData(int arg1, int arg2, const QString &arg3) { v = m_wnd->browser(arg1)->itemData(arg2, arg3); }

    QString sendKeys(const QString &arg1) { return m_wnd->sendKeys(arg1); }

private:
    QSharedPointer<MainWindow> m_wnd;
    QVariant v; ///< Last return value retrieved.
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
    explicit ScriptableProxy(const QSharedPointer<MainWindow> &mainWindow)
        : m_helper(new detail::ScriptableProxyHelper(mainWindow))
    {
        qRegisterMetaType<QSystemTrayIcon::MessageIcon>("SystemTrayIcon::MessageIcon");
        m_helper->moveToThread(mainWindow->thread());
    }

    ~ScriptableProxy() { delete m_helper; }

    PROXY_METHOD(close)
    PROXY_METHOD(showWindow)
    PROXY_METHOD(pasteToCurrentWindow)

    PROXY_METHOD_VOID_1(disableMonitoring, bool)
    PROXY_METHOD_VOID_1(setClipboard, const ClipboardItem *)

    PROXY_METHOD_VOID_2(renameTab, const QString &, int)
    PROXY_METHOD_VOID_2(removeTab, bool, int)

    PROXY_METHOD_0(QStringList, tabs)
    PROXY_METHOD_0(bool, toggleVisible)
    PROXY_METHOD_0(bool, toggleMenu)
    PROXY_METHOD_0(qulonglong, mainWinId)
    PROXY_METHOD_0(qulonglong, trayMenuWinId)
    PROXY_METHOD_1(int, findTabIndex, const QString &)
    PROXY_METHOD_VOID_1(createTab, const QString &)

    PROXY_METHOD_VOID_1(showBrowser, int)

    PROXY_METHOD_1(qulonglong, openActionDialog, const QVariantMap &)
    PROXY_METHOD_VOID_2(action, const QVariantMap &, const Command &)

    PROXY_METHOD_1(bool, loadTab, const QString &)
    PROXY_METHOD_2(bool, saveTab, const QString &, int)

    PROXY_METHOD_2(QVariant, config, const QString &, const QString &)

    PROXY_METHOD_VOID_4(showMessage, const QString &, const QString &,
                        QSystemTrayIcon::MessageIcon, int)

    PROXY_METHOD_1(QByteArray, getClipboardData, const QString &)
    PROXY_METHOD_2(QByteArray, getClipboardData, const QString &, QClipboard::Mode)

    PROXY_METHOD_VOID_1(browserLock, int)
    PROXY_METHOD_VOID_1(browserUnlock, int)
    PROXY_METHOD_VOID_1(browserCopyNextItemToClipboard, int)
    PROXY_METHOD_VOID_1(browserCopyPreviousItemToClipboard, int)
    PROXY_METHOD_VOID_2(browserMoveToClipboard, int, int)
    PROXY_METHOD_VOID_1(browserDelayedSaveItems, int)
    PROXY_METHOD_VOID_2(browserRemoveRow, int, int)
    PROXY_METHOD_VOID_2(browserSetCurrent, int, int)
    PROXY_METHOD_1(int, browserLength, int)
    PROXY_METHOD_2(bool, browserOpenEditor, int, const QByteArray &)

    PROXY_METHOD_2(bool, browserAdd, int, const QString &)
    PROXY_METHOD_3(bool, browserAdd, int, const QVariantMap &, int)
    PROXY_METHOD_VOID_2(browserEditRow, int, int)
    PROXY_METHOD_VOID_2(browserEditNew, int, const QString &)

    PROXY_METHOD_3(QByteArray, browserItemData, int, int, const QString &)

    PROXY_METHOD_1(QString, sendKeys, const QString &)

private:
    detail::ScriptableProxyHelper *m_helper; ///< For retrieving return values of methods in MainWindow.
};

#endif // SCRIPTABLEPROXY_H

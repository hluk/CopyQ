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
#include "gui/mainwindow.h"
#include "item/clipboarditem.h"

#include <QMetaObject>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>

Q_DECLARE_METATYPE(QSystemTrayIcon::MessageIcon)

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

#define BROWSER(call) \
    ClipboardBrowser *c = fetchBrowser(); \
    if (c) \
        c->call

#define BROWSER_RESULT(call) \
    ClipboardBrowser *c = fetchBrowser(); \
    v = c ? QVariant(c->call) : QVariant()

namespace detail {

class ScriptableProxyHelper : public QObject
{
    Q_OBJECT
public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    explicit ScriptableProxyHelper(const QSharedPointer<MainWindow> &mainWindow)
      : QObject(NULL)
      , m_wnd(mainWindow)
      , m_tabName()
      , m_lock()
    {
    }

    const QVariant &value() const { return v; }

    static QString tabNotFoundError()
    {
        return ScriptableProxyHelper::tr("Tab with given name doesn't exist!");
    }

    static QString tabNameEmptyError()
    {
        return ScriptableProxyHelper::tr("Tab name cannot be empty!");
    }

public slots:
    void close() { m_wnd->close(); }
    void showWindow() { m_wnd->showWindow(); }
    void pasteToCurrentWindow() { m_wnd->pasteToCurrentWindow(); }

    void disableMonitoring(bool arg1) { m_wnd->disableClipboardStoring(arg1); }
    void setClipboard(const QVariantMap &arg1) { m_wnd->setClipboard(arg1); }

    void renameTab(const QString &arg1, const QString &arg2)
    {
        if ( arg1.isEmpty() || arg2.isEmpty() ) {
            v = tabNameEmptyError();
            return;
        }

        const int i = m_wnd->findTabIndex(arg2);
        if (i == -1) {
            v = tabNotFoundError();
            return;
        }

        if ( m_wnd->findTabIndex(arg1) != -1 ) {
            v = tr("Tab with given name already exists!");
            return;
        }

        m_wnd->renameTab(arg1, i);
        v = QString();
    }

    void removeTab(const QString &arg1)
    {
        if ( arg1.isEmpty() ) {
            v = tabNameEmptyError();
            return;
        }

        const int i = m_wnd->findTabIndex(arg1);
        if (i == -1) {
            v = tabNotFoundError();
            return;
        }

        m_wnd->removeTab(false, i);
        v = QString();
    }

    void showBrowser()
    {
        ClipboardBrowser *c = fetchBrowser();
        if (c)
            m_wnd->showBrowser(c);
    }

    void action(const QVariantMap &arg1, const Command &arg2) { m_wnd->action(arg1, arg2); }

    void showMessage(const QString &arg1, const QString &arg2, QSystemTrayIcon::MessageIcon arg3, int arg4) { m_wnd->showMessage(arg1, arg2, arg3, arg4); }

    void browserLock()
    {
        Q_ASSERT(m_lock.isNull());
        ClipboardBrowser *c = fetchBrowser();
        if (c)
            m_lock.reset( new ClipboardBrowser::Lock(c) );
    }

    void browserUnlock()
    {
        Q_ASSERT(!m_lock.isNull());
        m_lock.reset();
    }

    void browserCopyNextItemToClipboard() { BROWSER(copyNextItemToClipboard()); }
    void browserCopyPreviousItemToClipboard() { BROWSER(copyPreviousItemToClipboard()); }
    void browserMoveToClipboard(int arg1) { BROWSER(moveToClipboard(arg1)); }
    void browserDelayedSaveItems() { BROWSER(delayedSaveItems()); }
    void browserRemoveRow(int arg1) { BROWSER(removeRow(arg1)); }
    void browserSetCurrent(int arg1) { BROWSER(setCurrent(arg1)); }

    void browserEditRow(int arg1) { BROWSER(editRow(arg1)); }
    void browserEditNew(const QString &arg1) { BROWSER(editNew(arg1)); }

    void tabs() { v = m_wnd->tabs(); }
    void toggleVisible() { v = m_wnd->toggleVisible(); }
    void toggleMenu() { v = m_wnd->toggleMenu(); }
    void mainWinId() { v = (qulonglong)m_wnd->mainWinId(); }
    void trayMenuWinId() { v = (qulonglong)m_wnd->trayMenuWinId(); }
    void findTabIndex(const QString &arg1) { v = m_wnd->findTabIndex(arg1); }

    void openActionDialog(const QVariantMap &arg1) { v = (qulonglong)m_wnd->openActionDialog(arg1); }

    void loadTab(const QString &arg1) { v = m_wnd->loadTab(arg1); }
    void saveTab(const QString &arg1)
    {
        ClipboardBrowser *c = fetchBrowser();
        if (c) {
            const int i = m_wnd->tabIndex(c);
            v = m_wnd->saveTab(arg1, i);
        }
    }

    void config(const QString &arg1, const QString &arg2) { v = m_wnd->config(arg1, arg2); }

    void getClipboardData(const QString &arg1) { v = m_wnd->getClipboardData(arg1); }
    void getClipboardData(const QString &arg1, QClipboard::Mode arg2) { v = m_wnd->getClipboardData(arg1, arg2); }

    void browserLength() { BROWSER_RESULT(length()); }
    void browserOpenEditor(const QByteArray &arg1) { BROWSER_RESULT(openEditor(arg1)); }

    void browserAdd(const QString &arg1) { BROWSER_RESULT(add(arg1)); }
    void browserAdd(const QVariantMap &arg1, int arg2) { BROWSER_RESULT(add(arg1, arg2)); }

    void browserItemData(int arg1, const QString &arg2) { BROWSER_RESULT(itemData(arg1, arg2)); }

    void sendKeys(const QString &arg1) { v = m_wnd->sendKeys(arg1); }

    void setCurrentTab(const QString &tabName) { m_tabName = tabName; }

    void currentTab() { BROWSER_RESULT(tabName()); }

    void selected() { v = m_wnd->selectedTab() + '\n' + m_wnd->selectedItems(); }
    void selectedTab() { v = m_wnd->selectedTab(); }
    void selectedItems() { v = m_wnd->selectedItems(); }

private:
    ClipboardBrowser *fetchBrowser()
    {
        ClipboardBrowser *c = m_tabName.isEmpty() ? m_wnd->browser(0) : m_wnd->createTab(m_tabName);
        if (!c)
            return NULL;

        c->loadItems();
        return c->isLoaded() ? c : NULL;
    }

    QSharedPointer<MainWindow> m_wnd;
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
    PROXY_METHOD_VOID_1(setClipboard, const QVariantMap &)

    PROXY_METHOD_2(QString, renameTab, const QString &, const QString &)
    PROXY_METHOD_1(QString, removeTab, const QString &)

    PROXY_METHOD_0(QStringList, tabs)
    PROXY_METHOD_0(bool, toggleVisible)
    PROXY_METHOD_0(bool, toggleMenu)
    PROXY_METHOD_0(qulonglong, mainWinId)
    PROXY_METHOD_0(qulonglong, trayMenuWinId)
    PROXY_METHOD_1(int, findTabIndex, const QString &)

    PROXY_METHOD(showBrowser)

    PROXY_METHOD_1(qulonglong, openActionDialog, const QVariantMap &)
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
    PROXY_METHOD(browserCopyNextItemToClipboard)
    PROXY_METHOD(browserCopyPreviousItemToClipboard)
    PROXY_METHOD_VOID_1(browserMoveToClipboard, int)
    PROXY_METHOD(browserDelayedSaveItems)
    PROXY_METHOD_VOID_1(browserRemoveRow, int)
    PROXY_METHOD_VOID_1(browserSetCurrent, int)
    PROXY_METHOD_0(int, browserLength)
    PROXY_METHOD_1(bool, browserOpenEditor, const QByteArray &)

    PROXY_METHOD_1(bool, browserAdd, const QString &)
    PROXY_METHOD_2(bool, browserAdd, const QVariantMap &, int)
    PROXY_METHOD_VOID_1(browserEditRow, int)
    PROXY_METHOD_VOID_1(browserEditNew, const QString &)

    PROXY_METHOD_2(QByteArray, browserItemData, int, const QString &)

    PROXY_METHOD_1(QString, sendKeys, const QString &)

    PROXY_METHOD_VOID_1(setCurrentTab, const QString &)
    PROXY_METHOD_0(QString, currentTab)

    PROXY_METHOD_0(QString, selected)
    PROXY_METHOD_0(QString, selectedTab)
    PROXY_METHOD_0(QString, selectedItems)

private:
    detail::ScriptableProxyHelper *m_helper; ///< For retrieving return values of methods in MainWindow.
};

#endif // SCRIPTABLEPROXY_H

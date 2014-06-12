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

#include "common/log.h"
#include "gui/clipboardbrowser.h"
#include "gui/mainwindow.h"
#include "item/clipboarditem.h"
#include "platform/platformnativeinterface.h"

#include <QDialog>
#include <QMetaObject>
#include <QObject>
#include <QStringList>

#ifdef HAS_TESTS
#   include <QTest>
#endif

Q_DECLARE_METATYPE(QSystemTrayIcon::MessageIcon)
Q_DECLARE_METATYPE(QList<int>)

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

inline QByteArray serializeWindow(WId winId)
{
    QByteArray data;
    return createPlatformNativeInterface()->serialize(winId, &data) ? data : QByteArray();
}

class ScriptableProxyHelper : public QObject
{
    Q_OBJECT
public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    explicit ScriptableProxyHelper(MainWindow* mainWindow)
      : QObject(NULL)
      , m_wnd(mainWindow)
      , m_tabName()
      , m_lock()
    {
        qRegisterMetaType< QPointer<QWidget> >("QPointer<QWidget>");
        moveToThread(m_wnd->thread());
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

    void ignoreCurrentClipboard() { m_wnd->ignoreCurrentClipboard(); }

    void isMonitoringEnabled() { v = m_wnd->isMonitoringEnabled(); }
    void disableMonitoring(bool arg1) { m_wnd->disableClipboardStoring(arg1); }
    void setClipboard(const QVariantMap &arg1) { m_wnd->setClipboard(arg1); }

    void renameTab(const QString &arg1, const QString &arg2)
    {
        v = QString();

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
    }

    void removeTab(const QString &arg1)
    {
        v = QString();

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
    }

    void showBrowser(const QString &tabName)
    {
        ClipboardBrowser *c = fetchBrowser(tabName);
        if (c)
            m_wnd->showBrowser(c);
    }

    void showBrowser() { showBrowser(m_tabName); }

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
    void browserSetCurrent(int arg1) { BROWSER(setCurrent(arg1)); }
    void browserRemoveRows(QList<int> rows)
    {
        ClipboardBrowser *c = fetchBrowser();
        if (!c)
            return;

        qSort( rows.begin(), rows.end(), qGreater<int>() );

        ClipboardBrowser::Lock lock(c);
        foreach (int row, rows)
            c->removeRow(row);
    }

    void browserEditRow(int arg1) { BROWSER(editRow(arg1)); }
    void browserEditNew(const QString &arg1) { BROWSER(editNew(arg1)); }

    void tabs() { v = m_wnd->tabs(); }
    void toggleVisible() { v = m_wnd->toggleVisible(); }
    void toggleMenu(const QString &tabName) { v = m_wnd->toggleMenu(fetchBrowser(tabName)); }
    void toggleMenu() { v = m_wnd->toggleMenu(); }
    void mainWinId() { v = serializeWindow(m_wnd->winId()); }
    void trayMenuWinId() { v = serializeWindow(m_wnd->trayMenu()->winId()); }
    void findTabIndex(const QString &arg1) { v = m_wnd->findTabIndex(arg1); }

    void openActionDialog(const QVariantMap &arg1) { v = (qulonglong)m_wnd->openActionDialog(arg1); }

    void loadTab(const QString &arg1) { v = m_wnd->loadTab(arg1); }
    void saveTab(const QString &arg1)
    {
        v = QString();
        ClipboardBrowser *c = fetchBrowser();
        if (c) {
            const int i = m_wnd->findTabIndex( c->tabName() );
            v = m_wnd->saveTab(arg1, i);
        }
    }

    void config(const QString &arg1, const QString &arg2) { v = m_wnd->config(arg1, arg2); }

    void getClipboardData(const QString &arg1) { v = m_wnd->getClipboardData(arg1); }
    void getClipboardData(const QString &arg1, QClipboard::Mode arg2) { v = m_wnd->getClipboardData(arg1, arg2); }

    void getActionData(const QByteArray &arg1, const QString &arg2) { v = m_wnd->getActionData(arg1, arg2); }

    void browserLength() { BROWSER_RESULT(length()); }
    void browserOpenEditor(const QByteArray &arg1) { BROWSER_RESULT(openEditor(arg1)); }

    void browserAdd(const QString &arg1) { BROWSER_RESULT(add(arg1)); }
    void browserAdd(const QVariantMap &arg1, int arg2) { BROWSER_RESULT(add(arg1, arg2)); }
    void browserAdd(const QStringList &texts) {
        ClipboardBrowser *c = fetchBrowser();
        if (!c) {
            v = false;
            return;
        }

        v = true;

        ClipboardBrowser::Lock lock(c);
        foreach (const QString &text, texts) {
            if ( !c->add(text) ) {
                v = false;
                return;
            }
        }

        return;
    }

    void browserItemData(int arg1, const QString &arg2) { BROWSER_RESULT(itemData(arg1, arg2)); }
    void browserItemData(int arg1) { BROWSER_RESULT(itemData(arg1)); }

    void setCurrentTab(const QString &tabName) { m_tabName = tabName; }

    void currentTab() { BROWSER_RESULT(tabName()); }

    void currentItem() { BROWSER_RESULT(currentIndex().row()); }
    void selectItems(const QList<int> &items)
    {
        v = false;

        ClipboardBrowser *c = fetchBrowser();
        if (!c)
            return;

        v = true;

        c->clearSelection();

        if ( !items.isEmpty() ) {
            c->setCurrent(items.last());

            foreach (int i, items) {
                const QModelIndex index = c->index(i);
                if (index.isValid())
                    c->selectionModel()->select(index, QItemSelectionModel::Select);
            }
        }
    }

    void selectedTab() { v = m_wnd->selectedTab(); }
    void selectedItems() { v = QVariant::fromValue(m_wnd->selectedItems()); }
    void index() { BROWSER_RESULT(currentIndex().row()); }

    void sendKeys(const QString &keys)
    {
#ifdef HAS_TESTS
        v = QString();

        if (keys == "FLUSH_KEYS")
            return;

        QWidget *w = m_wnd->trayMenu();

        if ( !w->isVisible() ) {
            w = QApplication::focusWidget();
            if (!w) {
                COPYQ_LOG("No focused widget -> using main window");
                w = m_wnd;
            }
        }

        if (keys.startsWith(":")) {
            const QString widgetName = QString("%1 in %2")
                    .arg(w->metaObject()->className())
                    .arg(w->window()->metaObject()->className());

            COPYQ_LOG( QString("Sending keys \"%1\" to \"%2\".")
                       .arg(keys.mid(1))
                       .arg(widgetName) );

            QTest::keyClicks(w, keys.mid(1), Qt::NoModifier, 50);
        } else {
            const QKeySequence shortcut(keys, QKeySequence::PortableText);

            if ( shortcut.isEmpty() ) {
                v = QString("Cannot parse key \"%1\"!").arg(keys);
                return;
            }

            // Don't stop when modal window is open.
            QMetaObject::invokeMethod( this, "keyClick", Qt::QueuedConnection,
                                       Q_ARG(const QKeySequence &, shortcut),
                                       Q_ARG(const QPointer<QWidget> &, w)
                                      );
        }
#else
        Q_UNUSED(keys);
        v = QString("This is only available if tests are compiled!");
#endif
    }

    void keyClick(const QKeySequence &shortcut, const QPointer<QWidget> &widget)
    {
#ifdef HAS_TESTS
        const QString keys = shortcut.toString();

        if (widget.isNull()) {
            COPYQ_LOG( QString("Failed to send key \"%1\".").arg(keys) );
            return;
        }

        const QString widgetName = QString("%1 in %2")
                .arg(widget->metaObject()->className())
                .arg(widget->window()->metaObject()->className());

        m_wnd->showMessage( widgetName, shortcut.toString(),
                            QSystemTrayIcon::Information, 4000 );

        COPYQ_LOG( QString("Sending key \"%1\" to \"%2\".")
                   .arg(keys)
                   .arg(widgetName) );

        QTest::keyClick( widget.data(),
                         Qt::Key(shortcut[0] & ~Qt::KeyboardModifierMask),
                         Qt::KeyboardModifiers(shortcut[0] & Qt::KeyboardModifierMask), 0 );

        COPYQ_LOG( QString("Key \"%1\" sent to \"%2\".")
                   .arg(keys)
                   .arg(widgetName) );
#else
        Q_UNUSED(shortcut);
        Q_UNUSED(widget);
#endif
    }

private:
    ClipboardBrowser *fetchBrowser(const QString &tabName)
    {
        ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser(0) : m_wnd->createTab(tabName);
        if (!c)
            return NULL;

        c->loadItems();
        return c->isLoaded() ? c : NULL;
    }

    ClipboardBrowser *fetchBrowser() { return fetchBrowser(m_tabName); }

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
    explicit ScriptableProxy(MainWindow *mainWindow)
        : m_helper(new detail::ScriptableProxyHelper(mainWindow))
    {
        qRegisterMetaType<QSystemTrayIcon::MessageIcon>("SystemTrayIcon::MessageIcon");
    }

    ~ScriptableProxy() { delete m_helper; }

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

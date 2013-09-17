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
#include <QMimeData>
#include <QStringList>

#define BEGIN_INVOKE(methodName) \
        QMetaObject::invokeMethod(m_wnd, methodName, Qt::BlockingQueuedConnection

#define END_INVOKE );

#define PROXY_METHOD(methodName) \
    void methodName() \
    { \
        BEGIN_INVOKE(#methodName) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_1(methodName, Arg1Type) \
    void methodName(Arg1Type arg1) \
    { \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_2(methodName, Arg1Type, Arg2Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        BEGIN_INVOKE(#methodName) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE \
    }

#define PROXY_METHOD_VOID_4(methodName, Arg1Type, Arg2Type, Arg3Type, Arg4Type) \
    void methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3, Arg4Type arg4) \
    { \
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
        RetType retVal; \
        BEGIN_INVOKE(#methodName) \
            , Q_RETURN_ARG(RetType, retVal) \
        END_INVOKE \
        return retVal; \
    }

#define PROXY_METHOD_1(RetType, methodName, Arg1Type) \
    RetType methodName(Arg1Type arg1) \
    { \
        RetType retVal; \
        BEGIN_INVOKE(#methodName) \
            , Q_RETURN_ARG(RetType, retVal) \
            , Q_ARG(Arg1Type, arg1) \
        END_INVOKE \
        return retVal; \
    }

#define PROXY_METHOD_2(RetType, methodName, Arg1Type, Arg2Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2) \
    { \
        RetType retVal; \
        BEGIN_INVOKE(#methodName) \
            , Q_RETURN_ARG(RetType, retVal) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
        END_INVOKE \
        return retVal; \
    }

#define PROXY_METHOD_3(RetType, methodName, Arg1Type, Arg2Type, Arg3Type) \
    RetType methodName(Arg1Type arg1, Arg2Type arg2, Arg3Type arg3) \
    { \
        RetType retVal; \
        BEGIN_INVOKE(#methodName) \
            , Q_RETURN_ARG(RetType, retVal) \
            , Q_ARG(Arg1Type, arg1) \
            , Q_ARG(Arg2Type, arg2) \
            , Q_ARG(Arg3Type, arg3) \
        END_INVOKE \
        return retVal; \
    }

/**
 * Invoke methods (of MainWindow and its ClipboardBrowser objects) from different thread.
 */
class ScriptableProxy
{
public:
    explicit ScriptableProxy(MainWindow *mainWindow) : m_wnd(mainWindow) {}

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
    PROXY_METHOD_0(WId, mainWinId)
    PROXY_METHOD_0(WId, trayMenuWinId)
    PROXY_METHOD_1(int, findTabIndex, const QString &)
    PROXY_METHOD_1(ClipboardBrowser *, createTab, const QString &)

    PROXY_METHOD_VOID_1(showBrowser, int)

    PROXY_METHOD_1(WId, openActionDialog, const QMimeData &)
    PROXY_METHOD_VOID_2(action, const QMimeData &, const Command &)

    PROXY_METHOD_1(bool, loadTab, const QString &)
    PROXY_METHOD_2(bool, saveTab, const QString &, int)

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
    PROXY_METHOD_3(bool, browserAdd, int, QMimeData *, int)
    PROXY_METHOD_VOID_2(browserEditRow, int, int)
    PROXY_METHOD_VOID_2(browserEditNew, int, const QString &)

    PROXY_METHOD_3(QByteArray, browserItemData, int, int, const QString &)

private:
    MainWindow *m_wnd;
};

#endif // SCRIPTABLEPROXY_H

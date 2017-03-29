/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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
#include "gui/notificationbutton.h"

#include <QClipboard>
#include <QList>
#include <QMetaObject>
#include <QObject>

#include <memory>

class MainWindow;
class QPersistentModelIndex;
class QPixmap;
class QPoint;

struct Command;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

using NamedValueList = QVector<NamedValue>;

class ScriptableProxy : public QObject
{
    Q_OBJECT
public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    explicit ScriptableProxy(MainWindow* mainWindow);

    const QVariant &value() const;

    void unsetValue();

    bool isValueUnset();

    QVariantMap getActionData(int id);
    void setActionData(int id, const QVariantMap &data);

    void exit();

    void close();
    bool showWindow();
    bool showWindowAt(const QRect &rect);
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
    void setTabIcon(const QString &tabName, const QString &icon);

    bool showBrowser(const QString &tabName);
    bool showBrowserAt(const QString &tabName, const QRect &rect);

    bool showBrowser();

    void action(const QVariantMap &arg1, const Command &arg2);

    void showMessage(const QString &title,
            const QString &msg,
            const QString &icon,
            int msec,
            const QString &notificationId = QString(),
            const NotificationButtons &buttons = NotificationButtons());

    QVariantMap nextItem(int where);
    void browserMoveToClipboard(int arg1);
    void browserSetCurrent(int arg1);
    QString browserRemoveRows(QList<int> rows);

    void browserEditRow(int arg1);
    void browserEditNew(const QString &arg1, bool changeClipboard);

    QStringList tabs();
    bool toggleVisible();
    bool toggleMenu(const QString &tabName, int maxItemCount, const QPoint &position);
    bool toggleMenu();
    int findTabIndex(const QString &arg1);

    void openActionDialog(const QVariantMap &arg1);

    bool loadTab(const QString &arg1);
    bool saveTab(const QString &arg1);

    bool importData(const QString &fileName);
    bool exportData(const QString &fileName);

    QStringList config(const QStringList &nameValue);

    QByteArray getClipboardData(const QString &mime, QClipboard::Mode mode = QClipboard::Clipboard);
    bool hasClipboardFormat(const QString &mime, QClipboard::Mode mode = QClipboard::Clipboard);

    int browserLength();
    bool browserOpenEditor(const QByteArray &arg1, bool changeClipboard);

    QString browserAdd(const QStringList &texts);
    QString browserAdd(const QVariantMap &arg1, int arg2);
    bool browserChange(const QVariantMap &data, int row);

    QByteArray browserItemData(int arg1, const QString &arg2);
    QVariantMap browserItemData(int arg1);

    void setCurrentTab(const QString &tabName);

    void setTab(const QString &tabName);

    QString tab();

    int currentItem();
    bool selectItems(const QList<int> &items);

    QList<int> selectedItems();

    int selectedItemsDataCount();
    QVariantMap selectedItemData(int selectedIndex);
    bool setSelectedItemData(int selectedIndex, const QVariantMap &data);

    QList<QVariantMap> selectedItemsData();
    void setSelectedItemsData(const QList<QVariantMap> &dataList);

#ifdef HAS_TESTS
    void sendKeys(const QString &keys, int delay);
    bool keysSent();
    QString testSelected();
    void resetTestSession(const QString &clipboardTabName);
#endif // HAS_TESTS

    QString currentWindowTitle();

    NamedValueList inputDialog(const NamedValueList &values);

    void setUserValue(const QString &key, const QVariant &value);

    void updateFirstItem(const QVariantMap &data);
    void updateTitle(const QVariantMap &data);
    void setSelectedItemsData(const QString &mime, const QVariant &value);

    void filter(const QString &text);

    QList<Command> commands();
    void setCommands(const QList<Command> &commands);
    void addCommands(const QList<Command> &commands);

    QByteArray screenshot(const QString &format, const QString &screenName, bool select);

    QString pluginsPath();
    QString themesPath();
    QString translationsPath();

signals:
    void sendMessage(const QByteArray &message, int messageCode);

private:
    ClipboardBrowser *fetchBrowser(const QString &tabName);
    ClipboardBrowser *fetchBrowser();

    QVariantMap itemData(int i);
    QByteArray itemData(int i, const QString &mime);

    ClipboardBrowser *currentBrowser() const;
    QList<QPersistentModelIndex> selectedIndexes() const;

    MainWindow* m_wnd;
    QString m_tabName;
    QVariantMap m_actionData;
    bool m_invoked;

    uint m_sentKeyClicks = 0;
};

QString pluginsPath();
QString themesPath();
QString translationsPath();

#endif // SCRIPTABLEPROXY_H

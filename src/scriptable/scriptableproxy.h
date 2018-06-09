/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "common/clipboardmode.h"
#include "common/command.h"
#include "gui/notificationbutton.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QVariant>
#include <QVector>

#include <memory>

class ClipboardBrowser;
class MainWindow;
class QPersistentModelIndex;
class QPixmap;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

using NamedValueList = QVector<NamedValue>;

struct ScriptablePath {
    QString path;
};

Q_DECLARE_METATYPE(NamedValueList)
Q_DECLARE_METATYPE(ScriptablePath)
Q_DECLARE_METATYPE(NotificationButtons)
Q_DECLARE_METATYPE(QVector<QVariantMap>)
Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(Command)
Q_DECLARE_METATYPE(ClipboardMode)

QDataStream &operator<<(QDataStream &out, const NotificationButton &button);
QDataStream &operator>>(QDataStream &in, NotificationButton &button);
QDataStream &operator<<(QDataStream &out, const NamedValueList &list);
QDataStream &operator>>(QDataStream &in, NamedValueList &list);
QDataStream &operator<<(QDataStream &out, const Command &command);
QDataStream &operator>>(QDataStream &in, Command &command);
QDataStream &operator<<(QDataStream &out, ClipboardMode mode);
QDataStream &operator>>(QDataStream &in, ClipboardMode &mode);
QDataStream &operator<<(QDataStream &out, const ScriptablePath &path);
QDataStream &operator>>(QDataStream &in, ScriptablePath &path);
QDataStream &operator<<(QDataStream &out, Qt::KeyboardModifiers value);
QDataStream &operator>>(QDataStream &in, Qt::KeyboardModifiers &value);

class ScriptableProxy : public QObject
{
    Q_OBJECT

public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    ScriptableProxy(MainWindow* mainWindow, QObject *parent);

    QByteArray callFunction(const QByteArray &serializedFunctionCall);

    int actionId() const { return m_actionId; }

public slots:
    void setReturnValue(const QByteArray &returnValue);

    QVariantMap getActionData(int id);
    void setActionData(int id, const QVariantMap &data);

    void exit();

    void close();
    bool showWindow();
    bool showWindowAt(QRect rect);
    bool pasteToCurrentWindow();
    bool copyFromCurrentWindow();

    bool isMonitoringEnabled();
    bool isMainWindowVisible();
    bool isMainWindowFocused();
    void disableMonitoring(bool arg1);
    void setClipboard(const QVariantMap &data, ClipboardMode mode);

    QString renameTab(const QString &arg1, const QString &arg2);

    QString removeTab(const QString &arg1);

    QString tabIcon(const QString &tabName);
    void setTabIcon(const QString &tabName, const QString &icon);

    bool showBrowser(const QString &tabName);
    bool showBrowserAt(const QString &tabName, QRect rect);

    bool showCurrentBrowser();

    void action(const QVariantMap &arg1, const Command &arg2);

    void runInternalAction(const QVariantMap &data, const QString &command);

    void showMessage(const QString &title,
            const QString &msg,
            const QString &icon,
            int msec,
            const QString &notificationId = QString(),
            const NotificationButtons &buttons = NotificationButtons());

    QVariantMap nextItem(int where);
    void browserMoveToClipboard(int row);
    void browserSetCurrent(int arg1);
    QString browserRemoveRows(QVector<int> rows);

    void browserEditRow(int arg1);
    void browserEditNew(const QString &arg1, bool changeClipboard);

    QStringList tabs();
    bool toggleVisible();
    bool toggleMenu(const QString &tabName, int maxItemCount, QPoint position);
    bool toggleCurrentMenu();
    int findTabIndex(const QString &arg1);

    void openActionDialog(const QVariantMap &arg1);

    bool loadTab(const QString &arg1);
    bool saveTab(const QString &arg1);

    bool importData(const QString &fileName);
    bool exportData(const QString &fileName);

    QVariant config(const QStringList &nameValue);
    QVariant toggleConfig(const QString &optionName);

    QByteArray getClipboardData(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);
    bool hasClipboardFormat(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);

    int browserLength();
    bool browserOpenEditor(const QByteArray &arg1, bool changeClipboard);

    QString browserInsert(int row, const QVector<QVariantMap> &items);
    bool browserChange(const QVariantMap &data, int row);

    QByteArray browserItemData(int arg1, const QString &arg2);
    QVariantMap browserItemData(int arg1);

    void setCurrentTab(const QString &tabName);

    void setTab(const QString &tabName);

    QString tab();

    int currentItem();
    bool selectItems(const QVector<int> &rows);

    QVector<int> selectedItems();

    int selectedItemsDataCount();
    QVariantMap selectedItemData(int selectedIndex);
    bool setSelectedItemData(int selectedIndex, const QVariantMap &data);

    QVector<QVariantMap> selectedItemsData();
    void setSelectedItemsData(const QVector<QVariantMap> &dataList);

#ifdef HAS_TESTS
    void sendKeys(const QString &keys, int delay);
    bool keysSent();
    QString testSelected();
#endif // HAS_TESTS

    void serverLog(const QString &text);

    QString currentWindowTitle();

    NamedValueList inputDialog(const NamedValueList &values);

    void setUserValue(const QString &key, const QVariant &value);

    void setSelectedItemsData(const QString &mime, const QVariant &value);

    void filter(const QString &text);

    QVector<Command> commands();
    void setCommands(const QVector<Command> &commands);
    void addCommands(const QVector<Command> &commands);

    QByteArray screenshot(const QString &format, const QString &screenName, bool select);

    QStringList screenNames();

    Qt::KeyboardModifiers queryKeyboardModifiers();

    QString pluginsPath();
    QString themesPath();
    QString translationsPath();

    QString iconColor();
    bool setIconColor(const QString &name);

    QString iconTag();
    void setIconTag(const QString &tag);

    QString iconTagColor();
    bool setIconTagColor(const QString &name);

    void setClipboardData(const QVariantMap &data);
    void setTitle(const QString &title);
    void setTitleForData(const QVariantMap &data);
    void saveData(const QString &tab, const QVariantMap &data);
    void showDataNotification(const QVariantMap &data);

    QStringList menuItemMatchCommands(int actionId);
    bool enableMenuItem(int actionId, int menuItemMatchCommandIndex, bool enabled);

    QVariantMap setDisplayData(int actionId, const QVariantMap &displayData);

    QVector<Command> automaticCommands();
    QVector<Command> displayCommands();
    QVector<Command> scriptCommands();

    bool openUrls(const QStringList &urls);

signals:
    void sendFunctionCall(const QByteArray &bytes);

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
    int m_actionId = -1;

    uint m_sentKeyClicks = 0;

    QVariant m_returnValue;
};

QString pluginsPath();
QString themesPath();
QString translationsPath();

#endif // SCRIPTABLEPROXY_H

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

#include "common/clipboardmode.h"
#include "common/command.h"
#include "gui/clipboardbrowser.h"
#include "gui/notificationbutton.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QVector>

#include <memory>

class MainWindow;
class QPersistentModelIndex;
class QPixmap;
class QPoint;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

using NamedValueList = QVector<NamedValue>;

Q_DECLARE_METATYPE(NamedValueList)
Q_DECLARE_METATYPE(NotificationButtons)
Q_DECLARE_METATYPE(QList<QVariantMap>)
Q_DECLARE_METATYPE(QVector<QVariantMap>)
Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(Command)
Q_DECLARE_METATYPE(ClipboardMode)

#if QT_VERSION < 0x050000
Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QVector<Command>)
#endif

QDataStream &operator<<(QDataStream &out, const NotificationButton &button);
QDataStream &operator>>(QDataStream &in, NotificationButton &button);
QDataStream &operator<<(QDataStream &out, const QList<QVariantMap> &list);
QDataStream &operator>>(QDataStream &in, QList<QVariantMap> &list);
QDataStream &operator<<(QDataStream &out, const NamedValueList &list);
QDataStream &operator>>(QDataStream &in, NamedValueList &list);
QDataStream &operator<<(QDataStream &out, const Command &command);
QDataStream &operator>>(QDataStream &in, Command &command);
QDataStream &operator<<(QDataStream &out, ClipboardMode mode);
QDataStream &operator>>(QDataStream &in, ClipboardMode &mode);

class ScriptableProxy : public QObject
{
    Q_OBJECT

public:
    /** Create proxy object and move it to same thread as @a mainWindow. */
    ScriptableProxy(MainWindow* mainWindow, QObject *parent);

    QByteArray callFunction(const QByteArray &serializedFunctionCall);

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

    bool enableMenuItem(int actionId, const Command &command, bool enabled);

    QVariantMap setDisplayData(int actionId, const QVariantMap &displayData);

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

    uint m_sentKeyClicks = 0;

    QVariant m_returnValue;
};

QString pluginsPath();
QString themesPath();
QString translationsPath();

#endif // SCRIPTABLEPROXY_H

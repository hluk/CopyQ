// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLEPROXY_H
#define SCRIPTABLEPROXY_H

#include "common/clipboardmode.h"
#include "common/command.h"
#include "gui/clipboardbrowser.h"
#include "gui/notificationbutton.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QRect>
#include <QVariant>
#include <QVector>

class ClipboardBrowser;
class KeyClicker;
class MainWindow;
class QEventLoop;
class QPersistentModelIndex;
class QPixmap;

struct NamedValue {
    NamedValue() {}
    NamedValue(const QString &name, const QVariant &value) : name(name), value(value) {}
    QString name;
    QVariant value;
};

struct VariantMapList {
    QVector<QVariantMap> items;
};

struct NamedValueList {
    QList<NamedValue> items;
};

struct NotificationButtonList {
    QList<NotificationButton> items;
};

struct KeyboardModifierList {
    Qt::KeyboardModifiers items;
};

struct ItemSelection {
    QPointer<ClipboardBrowser> browser;
    QList<QPersistentModelIndex> indexes;
};

Q_DECLARE_METATYPE(NamedValueList)
Q_DECLARE_METATYPE(NotificationButtonList)
Q_DECLARE_METATYPE(VariantMapList)
Q_DECLARE_METATYPE(KeyboardModifierList)

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
Q_DECLARE_METATYPE(ClipboardMode)
#endif

QDataStream &operator<<(QDataStream &out, const NotificationButtonList &list);
QDataStream &operator>>(QDataStream &in, NotificationButtonList &list);
QDataStream &operator<<(QDataStream &out, const NamedValueList &list);
QDataStream &operator>>(QDataStream &in, NamedValueList &list);
QDataStream &operator<<(QDataStream &out, const VariantMapList &list);
QDataStream &operator>>(QDataStream &in, VariantMapList &list);
QDataStream &operator<<(QDataStream &out, ClipboardMode mode);
QDataStream &operator>>(QDataStream &in, ClipboardMode &mode);
QDataStream &operator<<(QDataStream &out, KeyboardModifierList value);
QDataStream &operator>>(QDataStream &in, KeyboardModifierList &value);

class ScriptableProxy final : public QObject
{
    Q_OBJECT

public:
    explicit ScriptableProxy(MainWindow* mainWindow, QObject *parent = nullptr);

    void callFunction(const QByteArray &serializedFunctionCall);

    int actionId() const { return m_actionId; }

    void setFunctionCallReturnValue(const QByteArray &bytes);
    void setInputDialogResult(const QByteArray &bytes);

    void safeDeleteLater();

public slots:
    QVariantMap getActionData(int id);
    void setActionData(int id, const QVariantMap &data);

    void exit();

    void close();
    bool showWindow();
    bool showWindowAt(QRect rect);
    bool pasteToCurrentWindow();
    bool copyFromCurrentWindow();

    bool focusPrevious();

    bool isMonitoringEnabled();
    bool isMainWindowVisible();
    bool isMainWindowFocused();
    bool preview(const QVariant &arg);
    void disableMonitoring(bool arg1);
    void setClipboard(const QVariantMap &data, ClipboardMode mode);

    QString renameTab(const QString &arg1, const QString &arg2);

    QString removeTab(const QString &arg1);

    QString tabIcon(const QString &tabName);
    void setTabIcon(const QString &tabName, const QString &icon);

    QStringList unloadTabs(const QStringList &tabs);
    void forceUnloadTabs(const QStringList &tabs);

    bool showBrowser(const QString &tabName);
    bool showBrowserAt(const QString &tabName, QRect rect);

    void action(const QVariantMap &arg1, const Command &arg2);

    void runInternalAction(const QVariantMap &data, const QString &command);
    QByteArray tryGetCommandOutput(const QString &command);

    void showMessage(const QString &title,
            const QString &msg,
            const QString &icon,
            int msec,
            const QString &notificationId = QString(),
            const NotificationButtonList &buttons = NotificationButtonList());

    QVariantMap nextItem(const QString &tabName, int where);
    void browserMoveToClipboard(const QString &tabName, int row);
    void browserSetCurrent(const QString &tabName, int arg1);
    QString browserRemoveRows(const QString &tabName, QVector<int> rows);
    void browserMoveSelected(int targetRow);

    void browserEditRow(const QString &tabName, int arg1, const QString &format);
    void browserEditNew(const QString &tabName, const QString &format, const QByteArray &content, bool changeClipboard);

    QStringList tabs();
    bool toggleVisible();
    bool toggleMenu(const QString &tabName, int maxItemCount, QPoint position);
    bool toggleCurrentMenu();
    int findTabIndex(const QString &arg1);

    int menuItems(const VariantMapList &items);

    void openActionDialog(const QVariantMap &arg1);

    bool loadTab(const QString &arg1);
    bool saveTab(const QString &tabName, const QString &arg1);

    bool importData(const QString &fileName);
    bool exportData(const QString &fileName);

    QVariant config(const QVariantList &nameValue);
    QString configDescription();
    QVariant toggleConfig(const QString &optionName);

    int browserLength(const QString &tabName);
    bool browserOpenEditor(
        const QString &tabName, int row, const QString &format, const QByteArray &content, bool changeClipboard);

    QString browserInsert(const QString &tabName, int row, const VariantMapList &items);
    QString browserChange(const QString &tabName, int row, const VariantMapList &items);

    QByteArray browserItemData(const QString &tabName, int arg1, const QString &arg2);
    QVariantMap browserItemData(const QString &tabName, int arg1);

    void setCurrentTab(const QString &tabName);

    QString tab(const QString &tabName);

    int currentItem();
    bool selectItems(const QString &tabName, const QVector<int> &rows);

    QVector<int> selectedItems();
    QString selectedTab();

    QVariantMap selectedItemData(int selectedIndex);
    bool setSelectedItemData(int selectedIndex, const QVariantMap &data);

    VariantMapList selectedItemsData();
    void setSelectedItemsData(const VariantMapList &dataList);

    int createSelection(const QString &tabName);
    int selectionCopy(int id);
    void destroySelection(int id);
    void selectionRemoveAll(int id);
    void selectionSelectRemovable(int id);
    void selectionInvert(int id);
    void selectionSelectAll(int id);
    void selectionSelect(int id, const QVariant &maybeRe, const QString &mimeFormat);
    void selectionDeselectIndexes(int id, const QVector<int> &indexes);
    void selectionDeselectSelection(int id, int toDeselectId);
    void selectionGetCurrent(int id);
    int selectionGetSize(int id);
    QString selectionGetTabName(int id);
    QVector<int> selectionGetRows(int id);
    QVariantMap selectionGetItemIndex(int id, int index);
    void selectionSetItemIndex(int id, int index, const QVariantMap &item);
    QVariantList selectionGetItemsData(int id);
    void selectionSetItemsData(int id, const QVariantList &dataList);
    QVariantList selectionGetItemsFormat(int id, const QString &format);
    void selectionSetItemsFormat(int id, const QString &mime, const QVariant &value);
    void selectionMove(int id, int row);
    void selectionSort(int id, const QVector<int> &indexes);

#ifdef HAS_TESTS
    void sendKeys(const QString &expectedWidgetName, const QString &keys, int delay);
    bool sendKeysSucceeded();
    bool sendKeysFailed();
    QString testSelected();
#endif // HAS_TESTS

    void serverLog(const QString &text);

    QString currentWindowTitle();

    int inputDialog(const NamedValueList &values);

    void setSelectedItemsData(const QString &mime, const QVariant &value);

    void filter(const QString &text);
    QString filter();

    QVector<Command> commands();
    void setCommands(const QVector<Command> &commands);
    void addCommands(const QVector<Command> &commands);

    QByteArray screenshot(const QString &format, const QString &screenName, bool select);

    QStringList screenNames();

    KeyboardModifierList queryKeyboardModifiers();
    QPoint pointerPosition();
    void setPointerPosition(int x, int y);

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
    void saveData(const QString &tab, const QVariantMap &data, ClipboardMode mode);
    void showDataNotification(const QVariantMap &data);

    bool enableMenuItem(int actionId, int currentRun, int menuItemMatchCommandIndex, const QVariantMap &menuItem);

    QVariantMap setDisplayData(int actionId, const QVariantMap &displayData);

    QVector<Command> automaticCommands();
    QVector<Command> displayCommands();
    QVector<Command> scriptCommands();

    bool openUrls(const QStringList &urls);

    QString loadTheme(const QString &path);

    QByteArray getClipboardData(const QString &mime, ClipboardMode mode);
    bool hasClipboardFormat(const QString &mime, ClipboardMode mode);

    QStringList styles();

    void setScriptOverrides(const QVector<int> &overrides);

signals:
    void functionCallFinished(int functionCallId, const QVariant &returnValue);
    void inputDialogFinished(int dialogId, const NamedValueList &result);
    void sendMessage(const QByteArray &message, int messageCode);
    void clientDisconnected();
    void abortEvaluation();

private:
    ClipboardBrowser *fetchBrowser(const QString &tabName);

    QVariantMap itemData(const QString &tabName, int i);
    QByteArray itemData(const QString &tabName, int i, const QString &mime);

    void setItemsData(
        ClipboardBrowser *c, const QList<QPersistentModelIndex> &indexes, const QString &mime, const QVariant &value);

    template<typename T>
    T getSelectionData(const QString &mime);

    QPersistentModelIndex currentIndex();
    QList<QPersistentModelIndex> selectedIndexes();

    ClipboardBrowser *browserForIndexes(const QList<QPersistentModelIndex> &indexes) const;

    QVariant waitForFunctionCallFinished(int functionId);

    QByteArray callFunctionHelper(const QByteArray &serializedFunctionCall);

    bool getSelectionData();

#ifdef HAS_TESTS
    KeyClicker *keyClicker();
    KeyClicker *m_keyClicker = nullptr;
#endif // HAS_TESTS

    MainWindow* m_wnd;
    QVariantMap m_actionData;
    int m_actionId = -1;

    int m_lastFunctionCallId = -1;
    int m_lastInputDialogId = -1;

    int m_functionCallStack = 0;
    bool m_shouldBeDeleted = false;

    int m_lastSelectionId = -1;
    QMap<int, ItemSelection> m_selections;

    bool m_disconnected = false;
};

QString pluginsPath();
QString themesPath();
QString translationsPath();

void setClipboardMonitorRunning(bool running);
bool isClipboardMonitorRunning();

#endif // SCRIPTABLEPROXY_H

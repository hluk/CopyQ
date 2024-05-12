// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCRIPTABLE_H
#define SCRIPTABLE_H

#include "common/clipboardmode.h"
#include "common/command.h"
#include "common/mimetypes.h"

#include <QObject>
#include <QString>
#include <QJSValue>
#include <QVariantMap>
#include <QVector>

#include "platform/platformnativeinterface.h"

class Action;
class ScriptableByteArray;
class ClipboardBrowser;
class ItemFactory;
class NetworkReply;
class ScriptableProxy;

class QFile;
class QMimeData;
class QNetworkReply;
class QNetworkAccessManager;
class QJSEngine;

enum class ClipboardOwnership;

class Scriptable final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue inputSeparator READ getInputSeparator WRITE setInputSeparator)
    Q_PROPERTY(QJSValue mimeText READ getMimeText CONSTANT)
    Q_PROPERTY(QJSValue mimeTextUtf8 READ getMimeTextUtf8 CONSTANT)
    Q_PROPERTY(QJSValue mimeHtml READ getMimeHtml CONSTANT)
    Q_PROPERTY(QJSValue mimeUriList READ getMimeUriList CONSTANT)
    Q_PROPERTY(QJSValue mimeWindowTitle READ getMimeWindowTitle CONSTANT)
    Q_PROPERTY(QJSValue mimeItems READ getMimeItems CONSTANT)
    Q_PROPERTY(QJSValue mimeItemNotes READ getMimeItemNotes CONSTANT)
    Q_PROPERTY(QJSValue mimeIcon READ getMimeIcon CONSTANT)
    Q_PROPERTY(QJSValue mimeOwner READ getMimeOwner CONSTANT)
    Q_PROPERTY(QJSValue mimeClipboardMode READ getMimeClipboardMode CONSTANT)
    Q_PROPERTY(QJSValue mimeCurrentTab READ getMimeCurrentTab CONSTANT)
    Q_PROPERTY(QJSValue mimeSelectedItems READ getMimeSelectedItems CONSTANT)
    Q_PROPERTY(QJSValue mimeCurrentItem READ getMimeCurrentItem CONSTANT)
    Q_PROPERTY(QJSValue mimeHidden READ getMimeHidden CONSTANT)
    Q_PROPERTY(QJSValue mimeShortcut READ getMimeShortcut CONSTANT)
    Q_PROPERTY(QJSValue mimeColor READ getMimeColor CONSTANT)
    Q_PROPERTY(QJSValue mimeOutputTab READ getMimeOutputTab CONSTANT)
    Q_PROPERTY(QJSValue mimeDisplayItemInMenu READ getMimeDisplayItemInMenu CONSTANT)

    Q_PROPERTY(QJSValue plugins READ getPlugins CONSTANT)

    Q_PROPERTY(QJSValue _copyqUncaughtException READ uncaughtException WRITE setUncaughtException)
    Q_PROPERTY(QJSValue _copyqHasUncaughtException READ hasUncaughtException)

public:
    Scriptable(
            QJSEngine *engine,
            ScriptableProxy *proxy,
            ItemFactory *factory = nullptr,
            QObject *parent = nullptr);

    enum class Abort {
        None,
        CurrentEvaluation,
        AllEvaluations,
    };

    QJSValue argumentsArray() const;
    int argumentCount() const;
    QJSValue argument(int index) const;

    QJSValue newByteArray(const QByteArray &bytes) const;
    QJSValue newByteArray(ScriptableByteArray *ba) const;

    QByteArray fromString(const QString &value) const;
    QVariant toVariant(const QJSValue &value);
    bool toInt(const QJSValue &value, int *number) const;
    QVariantMap toDataMap(const QJSValue &value) const;
    QJSValue fromDataMap(const QVariantMap &dataMap) const;

    QByteArray makeByteArray(const QJSValue &value) const;

    /**
     * Set data for item converted from @a value.
     * Return true if data was successfully converted and set.
     *
     * If mime starts with "text/" or isn't byte array the value is re-encoded
     * from local encoding to UTF8.
     */
    bool toItemData(const QJSValue &value, const QString &mime, QVariantMap *data) const;

    QJSValue getInputSeparator() const;
    void setInputSeparator(const QJSValue &separator);

    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString getAbsoluteFilePath(const QString &fileName) const;

    QString arg(int i, const QString &defaultValue = QString());

    QJSValue throwError(const QString &errorMessage);
    QJSValue throwSaveError(const QString &filePath);
    QJSValue throwImportError(const QString &filePath);

    bool hasUncaughtException() const;
    void clearExceptions();
    QJSValue uncaughtException() const { return m_uncaughtException; }
    void setUncaughtException(const QJSValue &exc);

    QJSEngine *engine() const { return m_engine; }

    bool canContinue() const { return m_abort == Abort::None && !m_failed; }

    void installObject(QObject *fromObj, const QMetaObject *metaObject, QJSValue &toObject);

    QJSValue getMimeText() const { return mimeText; }
    QJSValue getMimeTextUtf8() const { return mimeTextUtf8; }
    QJSValue getMimeHtml() const { return mimeHtml; }
    QJSValue getMimeUriList() const { return mimeUriList; }
    QJSValue getMimeWindowTitle() const { return mimeWindowTitle; }
    QJSValue getMimeItems() const { return mimeItems; }
    QJSValue getMimeItemNotes() const { return mimeItemNotes; }
    QJSValue getMimeIcon() const { return mimeIcon; }
    QJSValue getMimeOwner() const { return mimeOwner; }
    QJSValue getMimeClipboardMode() const { return mimeClipboardMode; }
    QJSValue getMimeCurrentTab() const { return mimeCurrentTab; }
    QJSValue getMimeSelectedItems() const { return mimeSelectedItems; }
    QJSValue getMimeCurrentItem() const { return mimeCurrentItem; }
    QJSValue getMimeHidden() const { return mimeHidden; }
    QJSValue getMimeShortcut() const { return mimeShortcut; }
    QJSValue getMimeColor() const { return mimeColor; }
    QJSValue getMimeOutputTab() const { return mimeOutputTab; }
    QJSValue getMimeDisplayItemInMenu() const { return mimeDisplayItemInMenu; }

    QJSValue getPlugins();

    QJSValue eval(const QString &script, const QString &label);

    QJSValue call(const QString &label, QJSValue *fn, const QVariantList &arguments);
    QJSValue call(const QString &label, QJSValue *fn, const QJSValueList &arguments = QJSValueList());

    void setActionId(int actionId);
    void setActionName(const QString &actionName);
    int executeArguments(const QStringList &args);
    int executeArgumentsSimple(const QStringList &args);

    void abortEvaluation(Abort abort = Abort::AllEvaluations);

public slots:
    QJSValue ByteArray() const;
    QJSValue File() const;
    QJSValue TemporaryFile() const;
    QJSValue Dir() const;
    QJSValue ItemSelection() const;
    QJSValue Settings() const;

    QJSValue version();
    QJSValue help();

    void show();
    void showAt();
    void hide();
    QJSValue toggle();
    QJSValue menu();
    void exit();
    void disable();
    void enable();
    QJSValue monitoring();
    QJSValue visible();
    QJSValue focused();

    QJSValue focusPrevious();

    QJSValue preview();

    QJSValue filter();

    void ignore();

    QJSValue clipboard();
    QJSValue selection();
    QJSValue hasClipboardFormat();
    QJSValue hasSelectionFormat();
    QJSValue isClipboard();
    QJSValue copy();
    QJSValue copySelection();
    QJSValue paste();

    QJSValue tab();
    QJSValue removeTab();
    QJSValue removetab() { return removeTab(); }
    QJSValue renameTab();
    QJSValue renametab() { return renameTab(); }
    QJSValue tabIcon();
    QJSValue tabicon() { return tabIcon(); }
    QJSValue unload();
    void forceUnload();

    QJSValue length();
    QJSValue size() { return length(); }
    QJSValue count() { return length(); }

    QJSValue select();
    void next();
    void previous();
    void add();
    void insert();
    QJSValue remove();
    void edit();
    QJSValue editItem();
    QJSValue move();

    QJSValue read();
    QJSValue write();
    QJSValue change();
    void separator();

    void action();
    void popup();
    QJSValue notification();

    QJSValue exportTab();
    void exporttab() { exportTab(); }
    QJSValue importTab();
    void importtab() { importTab(); }

    QJSValue importData();
    QJSValue exportData();

    QJSValue config();
    QJSValue toggleConfig();

    QJSValue info();

    QJSValue eval();

    QJSValue source();

    QJSValue currentPath();
    QJSValue currentpath() { return currentPath(); }

    QJSValue str();
    QJSValue input();
    QJSValue toUnicode();
    QJSValue fromUnicode();

    QJSValue dataFormats();
    QJSValue data();
    QJSValue setData();
    QJSValue removeData();
    void print();
    void abort();
    void fail();

    QJSValue keys();
    QJSValue testSelected();
    void serverLog();
    QJSValue logs();

    void setCurrentTab();

    QJSValue selectItems();
    QJSValue selectitems() { return selectItems(); }

    QJSValue selectedTab();
    QJSValue selectedtab() { return selectedTab(); }
    QJSValue selectedItems();
    QJSValue selecteditems() { return selectedItems(); }
    QJSValue currentItem();
    QJSValue currentitem() { return currentItem(); }
    QJSValue index() { return currentItem(); }

    QJSValue selectedItemData();
    QJSValue setSelectedItemData();
    QJSValue selectedItemsData();
    void setSelectedItemsData();

    QJSValue escapeHtml();
    QJSValue escapeHTML() { return escapeHtml(); }

    QJSValue unpack();
    QJSValue pack();

    QJSValue getItem();
    QJSValue getitem() { return getItem(); }
    void setItem();
    void setitem() { setItem(); }

    QJSValue toBase64();
    QJSValue tobase64() { return toBase64(); }
    QJSValue fromBase64();
    QJSValue frombase64() { return fromBase64(); }

    QJSValue md5sum();
    QJSValue sha1sum();
    QJSValue sha256sum();
    QJSValue sha512sum();

    QJSValue open();
    QJSValue execute();

    QJSValue currentWindowTitle();
    QJSValue currentClipboardOwner();

    QJSValue dialog();

    QJSValue menuItems();

    QJSValue settings();

    QJSValue dateString();

    QJSValue commands();
    void setCommands();
    void addCommands();
    QJSValue importCommands();
    QJSValue exportCommands();

    QJSValue networkGet();
    QJSValue networkPost();
    QJSValue networkGetAsync();
    QJSValue networkPostAsync();

    QJSValue env();
    QJSValue setEnv();

    QJSValue sleep();
    QJSValue afterMilliseconds();

    // Call scriptable method.
    QVariant call(const QString &method, const QVariantList &arguments);
    QVariantList currentArguments();
    void throwException(const QString &errorMessage);

    QJSValue screenshot();
    QJSValue screenshotSelect();
    QJSValue screenNames();

    QJSValue queryKeyboardModifiers();
    QJSValue pointerPosition();
    QJSValue setPointerPosition();

    QJSValue iconColor();

    QJSValue iconTag();

    QJSValue iconTagColor();

    QJSValue loadTheme();

    void onClipboardChanged();
    void onOwnClipboardChanged();
    void onHiddenClipboardChanged();
    void onClipboardUnchanged();

    void onStart() {}
    void onExit() {}

    void synchronizeToSelection();
    void synchronizeFromSelection();

    void setClipboardData();
    void updateTitle();
    void setTitle();
    void saveData();
    QJSValue hasData();
    void showDataNotification();
    void hideDataNotification();
    void updateClipboardData();
    void clearClipboardData();
    QJSValue runAutomaticCommands();

    void runDisplayCommands();

    void runMenuCommandFilters();

    void monitorClipboard();
    void provideClipboard();
    void provideSelection();
    QJSValue isClipboardMonitorRunning();

    QJSValue clipboardFormatsToSave();

    QJSValue styles();

    void onItemsAdded() {}
    void onItemsRemoved() {}
    void onItemsChanged() {}
    void onTabSelected() {}
    void onItemsLoaded() {}
    void collectScriptOverrides();

signals:
    void finished();
    void dataReceived(const QByteArray &data);
    void receiveData();

private:
    void onExecuteOutput(const QByteArray &output);
    void onMonitorClipboardChanged(const QVariantMap &data, ClipboardOwnership ownership);
    void onMonitorClipboardUnchanged(const QVariantMap &data);
    void onSynchronizeSelection(ClipboardMode sourceMode, uint sourceTextHash, uint targetTextHash);
    void onFetchCurrentClipboardOwner(QString *title);

    bool sourceScriptCommands();
    void callDisplayFunctions(QJSValueList displayFunctions);
    void processUncaughtException(const QString &cmd);
    void showExceptionMessage(const QString &message);
    QVector<int> getRows() const;

    /**
     * Parses arguments as one of these or raises an argument error:
     * - item...
     * - mimeType, data, [mimeType, data]...
     * - list of items
     * - text
     */
    QVector<QVariantMap> getItemArguments(int begin, int end, QString *error);
    QVector<QVariantMap> getItemList(int begin, int end, const QJSValue &arguments);

    QJSValue copy(ClipboardMode mode);
    QJSValue changeItem(bool create);
    void nextToClipboard(int where);
    void editContent(int editRow, const QString &format, const QByteArray &content, bool changeClipboard);
    QJSValue screenshot(bool select);
    QByteArray serialize(const QJSValue &value);
    QJSValue eval(const QString &script);
    bool runAction(Action *action);
    bool runCommands(CommandType::CommandType type);
    bool canExecuteCommand(const Command &command, QStringList *arguments);
    bool canExecuteCommandFilter(const QString &matchCommand);
    bool verifyClipboardAccess();
    void provideClipboard(ClipboardMode mode);

    void insert(int argumentsEnd);
    void insert(int row, int argumentsBegin, int argumentsEnd);

    QStringList arguments();
    QVariantList argumentsAsVariants();

    void print(const QByteArray &message);
    void printError(const QByteArray &message);

    void getActionData();
    void getActionData(int actionId);
    void setActionData();

    QByteArray getClipboardData(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);
    bool hasClipboardFormat(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);

    bool canSynchronizeSelection(ClipboardMode targetMode);

    void saveData(const QString &tab);

    QJSValue readInput();

    PlatformClipboard *clipboardInstance();

    void interruptibleSleep(int msec);

    NetworkReply *networkGetHelper();
    NetworkReply *networkPostHelper();

    QJSValue newQObject(QObject *obj, const QJSValue &prototype) const;

    ScriptableProxy *m_proxy;
    QJSEngine *m_engine;
    ItemFactory *m_factory;
    QJSValue m_temporaryFileClass;
    QString m_inputSeparator;
    QJSValue m_input;
    QVariantMap m_data;
    QVariantMap m_oldData;
    int m_actionId = -1;
    QString m_actionName;
    Abort m_abort = Abort::None;
    int m_skipArguments = 0;

    // FIXME: Parameters for execute() shouldn't be global.
    QByteArray m_executeStdoutData;
    QString m_executeStdoutLastLine;
    QJSValue m_executeStdoutCallback;

    bool m_displayFunctionsLock = false;

    QJSValue m_plugins;

    Action *m_action = nullptr;
    bool m_failed = false;

    QString m_tabName;

    PlatformClipboardPtr m_clipboard;

    QJSValue m_uncaughtException;
    bool m_hasUncaughtException = false;

    QStringList m_stack;
    QStringList m_uncaughtExceptionStack;

    QJSValue m_safeCall;
    QJSValue m_safeEval;
    QJSValue m_createFn;
    QJSValue m_createFnB;
    QJSValue m_createProperty;

    QJSValue m_byteArrayPrototype;
    QJSValue m_filePrototype;
    QJSValue m_temporaryFilePrototype;
    QJSValue m_dirPrototype;
    QJSValue m_itemSelectionPrototype;
    QJSValue m_settingsPrototype;
};

class NetworkReply final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QJSValue data READ data CONSTANT)
    Q_PROPERTY(QJSValue error READ error CONSTANT)
    Q_PROPERTY(QJSValue status READ status CONSTANT)
    Q_PROPERTY(QJSValue redirect READ redirect CONSTANT)
    Q_PROPERTY(QJSValue headers READ headers CONSTANT)
    Q_PROPERTY(QJSValue finished READ finished CONSTANT)

public:
    static NetworkReply *get(const QString &url, Scriptable *scriptable);
    static NetworkReply *post(const QString &url, const QByteArray &postData, Scriptable *scriptable);

    ~NetworkReply();

    QJSValue data();

    QJSValue error();

    QJSValue status();
    QJSValue redirect();
    QJSValue headers();

    QJSValue finished();

    QJSValue toScriptValue();

private:
    explicit NetworkReply(const QString &url, const QByteArray &postData, Scriptable *scriptable);

    Scriptable *m_scriptable;
    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QJSValue m_data;
    QJSValue m_self;
    QByteArray m_rawData;
};

class ScriptablePlugins final : public QObject {
    Q_OBJECT

public:
    ScriptablePlugins(Scriptable *scriptable, ItemFactory *factory);

public slots:
    QJSValue load(const QString &name);

private:
    Scriptable *m_scriptable;
    ItemFactory *m_factory;
    QMap<QString, QJSValue> m_plugins;
};

#endif // SCRIPTABLE_H

/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#ifndef SCRIPTABLE_H
#define SCRIPTABLE_H

#include "common/clipboardmode.h"
#include "common/command.h"
#include "common/mimetypes.h"

#include <QObject>
#include <QString>
#include <QScriptable>
#include <QScriptValue>
#include <QVariantMap>
#include <QVector>

class Action;
class ByteArrayClass;
class ClipboardBrowser;
class DirClass;
class FileClass;
class ItemFactory;
class ScriptableProxy;
class TemporaryFileClass;

class QFile;
class QNetworkReply;
class QNetworkAccessManager;
class QScriptEngine;
class QTextCodec;

enum class ClipboardOwnership;

class Scriptable final : public QObject, protected QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QScriptValue inputSeparator READ getInputSeparator WRITE setInputSeparator)
    Q_PROPERTY(QScriptValue mimeText READ getMimeText CONSTANT)
    Q_PROPERTY(QScriptValue mimeHtml READ getMimeHtml CONSTANT)
    Q_PROPERTY(QScriptValue mimeUriList READ getMimeUriList CONSTANT)
    Q_PROPERTY(QScriptValue mimeWindowTitle READ getMimeWindowTitle CONSTANT)
    Q_PROPERTY(QScriptValue mimeItems READ getMimeItems CONSTANT)
    Q_PROPERTY(QScriptValue mimeItemNotes READ getMimeItemNotes CONSTANT)
    Q_PROPERTY(QScriptValue mimeIcon READ getMimeIcon CONSTANT)
    Q_PROPERTY(QScriptValue mimeOwner READ getMimeOwner CONSTANT)
    Q_PROPERTY(QScriptValue mimeClipboardMode READ getMimeClipboardMode CONSTANT)
    Q_PROPERTY(QScriptValue mimeCurrentTab READ getMimeCurrentTab CONSTANT)
    Q_PROPERTY(QScriptValue mimeSelectedItems READ getMimeSelectedItems CONSTANT)
    Q_PROPERTY(QScriptValue mimeCurrentItem READ getMimeCurrentItem CONSTANT)
    Q_PROPERTY(QScriptValue mimeHidden READ getMimeHidden CONSTANT)
    Q_PROPERTY(QScriptValue mimeShortcut READ getMimeShortcut CONSTANT)
    Q_PROPERTY(QScriptValue mimeColor READ getMimeColor CONSTANT)
    Q_PROPERTY(QScriptValue mimeOutputTab READ getMimeOutputTab CONSTANT)

    Q_PROPERTY(QScriptValue global READ getGlobal CONSTANT)
    Q_PROPERTY(QScriptValue plugins READ getPlugins CONSTANT)

public:
    explicit Scriptable(
            QScriptEngine *engine,
            ScriptableProxy *proxy,
            QObject *parent = nullptr);

    enum class Abort {
        None,
        CurrentEvaluation,
        AllEvaluations,
    };

    // WORKAROUND: These methods override the one in QScriptable,
    //             which doesn't work well in some cases.
    QScriptContext *context() const;
    int argumentCount() const;
    QScriptValue argument(int index) const;

    QScriptValue newByteArray(const QByteArray &bytes);

    QByteArray fromString(const QString &value) const;
    QVariant toVariant(const QScriptValue &value);
    bool toInt(const QScriptValue &value, int *number) const;
    QVariantMap toDataMap(const QScriptValue &value) const;
    QScriptValue fromDataMap(const QVariantMap &dataMap) const;

    QByteArray makeByteArray(const QScriptValue &value) const;

    /**
     * Set data for item converted from @a value.
     * Return true if data was successfully converted and set.
     *
     * If mime starts with "text/" or isn't byte array the value is re-encoded
     * from local encoding to UTF8.
     */
    bool toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const;

    QScriptValue getInputSeparator() const;
    void setInputSeparator(const QScriptValue &separator);

    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString getAbsoluteFilePath(const QString &fileName) const;

    QString arg(int i, const QString &defaultValue = QString());

    void throwError(const QString &errorMessage);
    void throwSaveError(const QString &filePath);
    void throwImportError(const QString &filePath);

    QScriptEngine *engine() const { return m_engine; }

    bool canContinue() const { return m_abort == Abort::None && !m_failed; }

    QScriptValue getMimeText() const { return mimeText; }
    QScriptValue getMimeHtml() const { return mimeHtml; }
    QScriptValue getMimeUriList() const { return mimeUriList; }
    QScriptValue getMimeWindowTitle() const { return mimeWindowTitle; }
    QScriptValue getMimeItems() const { return mimeItems; }
    QScriptValue getMimeItemNotes() const { return mimeItemNotes; }
    QScriptValue getMimeIcon() const { return mimeIcon; }
    QScriptValue getMimeOwner() const { return mimeOwner; }
    QScriptValue getMimeClipboardMode() const { return mimeClipboardMode; }
    QScriptValue getMimeCurrentTab() const { return mimeCurrentTab; }
    QScriptValue getMimeSelectedItems() const { return mimeSelectedItems; }
    QScriptValue getMimeCurrentItem() const { return mimeCurrentItem; }
    QScriptValue getMimeHidden() const { return mimeHidden; }
    QScriptValue getMimeShortcut() const { return mimeShortcut; }
    QScriptValue getMimeColor() const { return mimeColor; }
    QScriptValue getMimeOutputTab() const { return mimeOutputTab; }

    QScriptValue getGlobal();
    QScriptValue getPlugins();

    ByteArrayClass *byteArrayClass() const { return m_baClass; }
    FileClass *fileClass() const { return m_fileClass; }
    TemporaryFileClass *temporaryFileClass() const { return m_temporaryFileClass; }

    QScriptValue call(QScriptValue *fn, const QScriptValue &object, const QVariantList &arguments) const;

    QScriptValue eval(const QString &script, const QString &fileName);

    void setActionId(int actionId);
    void setActionName(const QString &actionName);
    int executeArguments(const QStringList &args);

    void abortEvaluation(Abort abort = Abort::AllEvaluations);

public slots:
    QScriptValue version();
    QScriptValue help();

    void show();
    void showAt();
    void hide();
    QScriptValue toggle();
    void menu();
    void exit();
    void disable();
    void enable();
    QScriptValue monitoring();
    QScriptValue visible();
    QScriptValue focused();

    QScriptValue preview();

    QScriptValue filter();

    void ignore();

    QScriptValue clipboard();
    QScriptValue selection();
    QScriptValue hasClipboardFormat();
    QScriptValue hasSelectionFormat();
    QScriptValue isClipboard();
    QScriptValue copy();
    QScriptValue copySelection();
    void paste();

    QScriptValue tab();
    void removeTab();
    void removetab() { removeTab(); }
    void renameTab();
    void renametab() { renameTab(); }
    QScriptValue tabIcon();
    QScriptValue tabicon() { return tabIcon(); }
    QScriptValue unload();
    void forceUnload();

    QScriptValue length();
    QScriptValue size() { return length(); }
    QScriptValue count() { return length(); }

    void select();
    void next();
    void previous();
    void add();
    void insert();
    void remove();
    void edit();
    void move();

    QScriptValue read();
    void write();
    void change();
    void separator();

    void action();
    void popup();
    void notification();

    void exportTab();
    void exporttab() { exportTab(); }
    void importTab();
    void importtab() { importTab(); }

    void importData();
    void exportData();

    QScriptValue config();
    bool toggleConfig();

    QScriptValue info();

    QScriptValue eval();

    QScriptValue source();

    QScriptValue currentPath();
    QScriptValue currentpath() { return currentPath(); }

    QScriptValue str(const QScriptValue &value);
    QScriptValue input();
    QScriptValue toUnicode();
    QScriptValue fromUnicode();

    QScriptValue dataFormats();
    QScriptValue data(const QScriptValue &value);
    QScriptValue setData();
    QScriptValue removeData();
    void print(const QScriptValue &value);
    void abort();
    void fail();

    void keys();
    QScriptValue testSelected();
    void serverLog();
    QScriptValue logs();

    void setCurrentTab();

    QScriptValue selectItems();
    QScriptValue selectitems() { return selectItems(); }

    QScriptValue selectedTab();
    QScriptValue selectedtab() { return selectedTab(); }
    QScriptValue selectedItems();
    QScriptValue selecteditems() { return selectedItems(); }
    QScriptValue currentItem();
    QScriptValue currentitem() { return currentItem(); }
    QScriptValue index() { return currentItem(); }

    QScriptValue selectedItemData();
    QScriptValue setSelectedItemData();
    QScriptValue selectedItemsData();
    void setSelectedItemsData();

    QScriptValue escapeHtml();
    QScriptValue escapeHTML() { return escapeHtml(); }

    QScriptValue unpack();
    QScriptValue pack();

    QScriptValue getItem();
    QScriptValue getitem() { return getItem(); }
    void setItem();
    void setitem() { setItem(); }

    QScriptValue toBase64();
    QScriptValue tobase64() { return toBase64(); }
    QScriptValue fromBase64();
    QScriptValue frombase64() { return fromBase64(); }

    QScriptValue md5sum();
    QScriptValue sha1sum();
    QScriptValue sha256sum();
    QScriptValue sha512sum();

    QScriptValue open();
    QScriptValue execute();

    QScriptValue currentWindowTitle();

    QScriptValue dialog();

    QScriptValue menuItems();

    QScriptValue settings();

    QScriptValue dateString();

    QScriptValue commands();
    void setCommands();
    void addCommands();
    QScriptValue importCommands();
    QScriptValue exportCommands();

    QScriptValue networkGet();
    QScriptValue networkPost();

    QScriptValue env();
    QScriptValue setEnv();

    void sleep();
    void afterMilliseconds();

    // Call scriptable method.
    QVariant call(const QString &method, const QVariantList &arguments);

    QVariantList currentArguments();

    QScriptValue screenshot();
    QScriptValue screenshotSelect();
    QScriptValue screenNames();

    QScriptValue queryKeyboardModifiers();
    QScriptValue pointerPosition();
    void setPointerPosition();

    QScriptValue iconColor();

    QScriptValue iconTag();

    QScriptValue iconTagColor();

    void loadTheme();

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
    QScriptValue hasData();
    void showDataNotification();
    void hideDataNotification();
    void updateClipboardData();
    void clearClipboardData();
    QScriptValue runAutomaticCommands();

    void runDisplayCommands();

    void runMenuCommandFilters();

    void monitorClipboard();
    void provideClipboard();
    void provideSelection();

    QScriptValue clipboardFormatsToSave();

signals:
    void finished();
    void dataReceived(const QByteArray &data);
    void receiveData();

private:
    void onExecuteOutput(const QByteArray &output);
    void onMonitorClipboardChanged(const QVariantMap &data, ClipboardOwnership ownership);
    void onMonitorClipboardUnchanged(const QVariantMap &data);
    void onSynchronizeSelection(ClipboardMode sourceMode, const QString &text, uint targetTextHash);

    bool sourceScriptCommands();
    void callDisplayFunctions(QScriptValueList displayFunctions);
    QString processUncaughtException(const QString &cmd);
    void showExceptionMessage(const QString &message);
    QVector<int> getRows() const;
    QScriptValue copy(ClipboardMode mode);
    void changeItem(bool create);
    void nextToClipboard(int where);
    QScriptValue screenshot(bool select);
    QByteArray serialize(const QScriptValue &value);
    QScriptValue eval(const QString &script);
    QTextCodec *codecFromNameOrThrow(const QScriptValue &codecName);
    bool runAction(Action *action);
    bool runCommands(CommandType::CommandType type);
    bool canExecuteCommand(const Command &command);
    bool canExecuteCommandFilter(const QString &matchCommand);
    bool canAccessClipboard() const;
    bool verifyClipboardAccess();
    void provideClipboard(ClipboardMode mode);

    void insert(int argumentsEnd);
    void insert(int row, int argumentsBegin, int argumentsEnd);

    QStringList arguments();

    void print(const QByteArray &message);
    void printError(const QByteArray &message);

    void getActionData();
    void getActionData(int actionId);
    void setActionData();

    QByteArray getClipboardData(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);
    bool hasClipboardFormat(const QString &mime, ClipboardMode mode = ClipboardMode::Clipboard);

    void synchronizeSelection(ClipboardMode targetMode);

    void saveData(const QString &tab);

    QScriptValue readInput();

    ScriptableProxy *m_proxy;
    QScriptEngine *m_engine;
    ByteArrayClass *m_baClass;
    DirClass *m_dirClass;
    FileClass *m_fileClass;
    TemporaryFileClass *m_temporaryFileClass;
    QString m_inputSeparator;
    QScriptValue m_input;
    QVariantMap m_data;
    QVariantMap m_oldData;
    int m_actionId = -1;
    QString m_actionName;
    Abort m_abort = Abort::None;
    int m_skipArguments = 0;

    // FIXME: Parameters for execute() shouldn't be global.
    QByteArray m_executeStdoutData;
    QString m_executeStdoutLastLine;
    QScriptValue m_executeStdoutCallback;

    bool m_displayFunctionsLock = false;

    QScriptValue m_plugins;

    Action *m_action = nullptr;
    bool m_failed = false;

    QString m_tabName;
};

class NetworkReply final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QScriptValue data READ data CONSTANT)
    Q_PROPERTY(QScriptValue error READ error CONSTANT)
    Q_PROPERTY(QScriptValue status READ status CONSTANT)
    Q_PROPERTY(QScriptValue redirect READ redirect CONSTANT)
    Q_PROPERTY(QScriptValue headers READ headers CONSTANT)

public:
    static QScriptValue get(const QString &url, Scriptable *scriptable);
    static QScriptValue post(const QString &url, const QByteArray &postData, Scriptable *scriptable);

    ~NetworkReply();

    QScriptValue data();

    QScriptValue error() const;

    QScriptValue status() const;
    QScriptValue redirect() const;
    QScriptValue headers();

private:
    explicit NetworkReply(const QString &url, const QByteArray &postData, Scriptable *scriptable);

    QScriptValue toScriptValue();

    void fetchHeaders();

    Scriptable *m_scriptable;
    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QNetworkReply *m_replyHead;
    QScriptValue m_data;
};

#endif // SCRIPTABLE_H

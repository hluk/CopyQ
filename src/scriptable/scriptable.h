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

#ifndef SCRIPTABLE_H
#define SCRIPTABLE_H

#include "common/mimetypes.h"

#include <QClipboard>
#include <QObject>
#include <QString>
#include <QScriptable>
#include <QScriptValue>
#include <QVariantMap>

class ByteArrayClass;
class ClipboardBrowser;
class DirClass;
class FileClass;
class ScriptableProxy;
class TemporaryFileClass;

class QFile;
class QNetworkReply;
class QNetworkAccessManager;
class QScriptEngine;
class QTextCodec;

class Scriptable : public QObject, protected QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QScriptValue inputSeparator READ getInputSeparator WRITE setInputSeparator)
    Q_PROPERTY(QScriptValue mimeText READ getMimeText)
    Q_PROPERTY(QScriptValue mimeHtml READ getMimeHtml)
    Q_PROPERTY(QScriptValue mimeUriList READ getMimeUriList)
    Q_PROPERTY(QScriptValue mimeWindowTitle READ getMimeWindowTitle)
    Q_PROPERTY(QScriptValue mimeItems READ getMimeItems)
    Q_PROPERTY(QScriptValue mimeItemNotes READ getMimeItemNotes)
    Q_PROPERTY(QScriptValue mimeOwner READ getMimeOwner)
    Q_PROPERTY(QScriptValue mimeClipboardMode READ getMimeClipboardMode)
    Q_PROPERTY(QScriptValue mimeCurrentTab READ getMimeCurrentTab)
    Q_PROPERTY(QScriptValue mimeSelectedItems READ getMimeSelectedItems)
    Q_PROPERTY(QScriptValue mimeCurrentItem READ getMimeCurrentItem)
    Q_PROPERTY(QScriptValue mimeHidden READ getMimeHidden)
    Q_PROPERTY(QScriptValue mimeShortcut READ getMimeShortcut)
    Q_PROPERTY(QScriptValue mimeColor READ getMimeColor)
    Q_PROPERTY(QScriptValue mimeOutputTab READ getMimeOutputTab)
    Q_PROPERTY(QScriptValue mimeSyncToClipboard READ getMimeSyncToClipboard)
    Q_PROPERTY(QScriptValue mimeSyncToSelection READ getMimeSyncToSelection)

public:
    explicit Scriptable(
            QScriptEngine *engine,
            ScriptableProxy *proxy,
            QObject *parent = nullptr);

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

    QByteArray makeByteArray(const QScriptValue &value) const;

    /**
     * Set data for item converted from @a value.
     * Return true if data was successfuly converted and set.
     *
     * If mime starts with "text/" or isn't byte array the value is re-encoded
     * from local encoding to UTF8.
     */
    bool toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const;

    QScriptValue getInputSeparator() const;
    void setInputSeparator(const QScriptValue &separator);

    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString getFileName(const QString &fileName) const;

    QString arg(int i, const QString &defaultValue = QString());

    void throwError(const QString &errorMessage);

    void sendMessageToClient(const QByteArray &message, int exitCode);

    QScriptEngine *engine() const { return m_engine; }

    bool isConnected() const { return m_connected; }

    const QVariantMap &data() const { return m_data; }

    QScriptValue getMimeText() const { return mimeText; }
    QScriptValue getMimeHtml() const { return mimeHtml; }
    QScriptValue getMimeUriList() const { return mimeUriList; }
    QScriptValue getMimeWindowTitle() const { return mimeWindowTitle; }
    QScriptValue getMimeItems() const { return mimeItems; }
    QScriptValue getMimeItemNotes() const { return mimeItemNotes; }
    QScriptValue getMimeOwner() const { return mimeOwner; }
    QScriptValue getMimeClipboardMode() const { return mimeClipboardMode; }
    QScriptValue getMimeCurrentTab() const { return mimeCurrentTab; }
    QScriptValue getMimeSelectedItems() const { return mimeSelectedItems; }
    QScriptValue getMimeCurrentItem() const { return mimeCurrentItem; }
    QScriptValue getMimeHidden() const { return mimeHidden; }
    QScriptValue getMimeShortcut() const { return mimeShortcut; }
    QScriptValue getMimeColor() const { return mimeColor; }
    QScriptValue getMimeOutputTab() const { return mimeOutputTab; }
    QScriptValue getMimeSyncToClipboard() const { return mimeSyncToClipboard; }
    QScriptValue getMimeSyncToSelection() const { return mimeSyncToSelection; }

    ByteArrayClass *byteArrayClass() const { return m_baClass; }
    FileClass *fileClass() const { return m_fileClass; }
    TemporaryFileClass *temporaryFileClass() const { return m_temporaryFileClass; }

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

    void filter();

    void ignore();

    QScriptValue clipboard();
    QScriptValue selection();
    QScriptValue hasClipboardFormat();
    QScriptValue hasSelectionFormat();
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

    QScriptValue importData();
    QScriptValue exportData();

    QScriptValue config();

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
    void resetTestSession();
    void flush();

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

    QScriptValue open();
    QScriptValue execute();

    QScriptValue currentWindowTitle();

    QScriptValue dialog();

    QScriptValue settings();

    QScriptValue dateString();

    void updateFirst();
    void updateTitle();

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

    // Call scriptable method.
    QVariant call(const QString &method, const QVariantList &arguments);

    QVariantList currentArguments();

    QScriptValue screenshot();
    QScriptValue screenshotSelect();

public slots:
    void onMessageReceived(const QByteArray &bytes, int messageCode);
    void onDisconnected();

signals:
    void sendMessage(const QByteArray &message, int messageCode);

private slots:
    void onExecuteOutput(const QStringList &lines);

private:
    void executeArguments(const QByteArray &bytes);
    QString processUncaughtException(const QString &cmd);
    void showExceptionMessage(const QString &message);
    QList<int> getRows() const;
    QScriptValue copy(QClipboard::Mode mode);
    bool setClipboard(QVariantMap *data, QClipboard::Mode mode);
    void changeItem(bool create);
    void nextToClipboard(int where);
    QScriptValue screenshot(bool select);
    QByteArray serialize(const QScriptValue &value);
    QScriptValue eval(const QString &script, const QString &fileName);
    QScriptValue eval(const QString &script);
    QTextCodec *codecFromNameOrThrow(const QScriptValue &codecName);

    ScriptableProxy *m_proxy;
    QScriptEngine *m_engine;
    ByteArrayClass *m_baClass;
    DirClass *m_dirClass;
    FileClass *m_fileClass;
    TemporaryFileClass *m_temporaryFileClass;
    QString m_inputSeparator;
    QScriptValue m_input;
    QVariantMap m_data;
    QString m_actionName;
    bool m_connected;
    int m_skipArguments = 0;

    QScriptValue m_executeStdoutCallback;
};

class NetworkReply : public QObject {
    Q_OBJECT
    Q_PROPERTY(QScriptValue data READ data)
    Q_PROPERTY(QScriptValue error READ error)
    Q_PROPERTY(QScriptValue status READ status)
    Q_PROPERTY(QScriptValue redirect READ redirect)
    Q_PROPERTY(QScriptValue headers READ headers)

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

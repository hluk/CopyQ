/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "scriptableproxy.h"

#include <QObject>
#include <QString>
#include <QScriptable>
#include <QScriptValue>

class ByteArrayClass;
class ClipboardBrowser;
class DirClass;
class FileClass;
class QFile;
class QNetworkReply;
class QNetworkAccessManager;
class QScriptEngine;

class Scriptable : public QObject, protected QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString inputSeparator READ getInputSeparator WRITE setInputSeparator)
    Q_PROPERTY(QString currentPath READ getCurrentPath WRITE setCurrentPath)

public:
    Scriptable(ScriptableProxy *proxy, QObject *parent = NULL);

    void initEngine(
            QScriptEngine *engine, const QString &currentPath, const QVariantMap &data);

    QScriptValue newByteArray(const QByteArray &bytes);

    QScriptValue newVariant(const QVariant &value);

    QByteArray fromString(const QString &value) const;
    QString toString(const QScriptValue &value) const;
    bool toInt(const QScriptValue &value, int &number) const;
    QVariantMap toDataMap(const QScriptValue &value) const;

    /**
     * Return pointer to QByteArray or NULL.
     */
    QByteArray *getByteArray(const QScriptValue &value) const;

    QByteArray makeByteArray(const QScriptValue &value) const;

    QFile *getFile(const QScriptValue &value) const;

    /**
     * Set data for item converted from @a value.
     * Return true if data was successfuly converted and set.
     *
     * If mime starts with "text/" or isn't byte array the value is re-encoded
     * from local encoding to UTF8.
     */
    bool toItemData(const QScriptValue &value, const QString &mime, QVariantMap *data) const;

    QScriptValue applyRest(int first);

    const QString &getInputSeparator() const;
    void setInputSeparator(const QString &separator);

    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString getFileName(const QString &fileName) const;

    QString arg(int i, const QString &defaultValue = QString());

    void throwError(const QString &errorMessage);

    void sendMessageToClient(const QByteArray &message, int exitCode);

    void sendWindowActivationCommandToClient(const QByteArray &message);

    QScriptEngine *engine() const { return m_engine; }

    bool isAborted() const { return m_abort; }

    const QVariantMap &data() const { return m_data; }

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
    QScriptValue separator();

    void action();
    void popup();

    void exportTab();
    void exporttab() { exportTab(); }
    void importTab();
    void importtab() { importTab(); }

    QScriptValue config();

    QScriptValue eval();

    QScriptValue currentPath();
    QScriptValue currentpath() { return currentPath(); }

    QScriptValue str(const QScriptValue &value);
    QScriptValue input();
    QScriptValue data(const QScriptValue &value);
    QScriptValue setData();
    void print(const QScriptValue &value);
    void abort();
    void fail();

    void keys();
    QScriptValue testSelected();

    void setCurrentTab();

    QScriptValue selectItems();
    QScriptValue selectitems() { return selectItems(); }

    QScriptValue selectedTab();
    QScriptValue selectedtab() { return selectedTab(); }
    QScriptValue selectedItems();
    QScriptValue selecteditems() { return selectedItems(); }
    QScriptValue currentItem();
    QScriptValue currentitem() { return currentItem(); }

    QScriptValue index();

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

    QScriptValue networkGet();
    QScriptValue networkPost();

    QScriptValue env();
    QScriptValue setEnv();

public slots:
    void setInput(const QByteArray &bytes);

signals:
    void sendMessage(const QByteArray &message, int messageCode);
    void requestApplicationQuit();

private:
    QList<int> getRows() const;
    QScriptValue copy(QClipboard::Mode mode);
    bool setClipboard(QVariantMap &data, QClipboard::Mode mode);
    void changeItem(bool create);
    void nextToClipboard(int where);

    ScriptableProxy *m_proxy;
    QScriptEngine *m_engine;
    ByteArrayClass *m_baClass;
    DirClass *m_dirClass;
    FileClass *m_fileClass;
    QString m_inputSeparator;
    QScriptValue m_input;
    QVariantMap m_data;
    bool m_abort;
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

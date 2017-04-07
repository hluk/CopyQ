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

#include <QClipboard>
#include <QJSValue>
#include <QObject>
#include <QVariantMap>

class QByteArray;
class QJSEngine;
class QTextCodec;
class ScriptableProxy;

class Scriptable : public QObject
{
    Q_OBJECT
public:
    Scriptable(QJSEngine *engine, ScriptableProxy *proxy);

    bool isConnected() const { return m_connected; }

    QJSValue arguments() const;
    int argumentCount() const;
    QJSValue argument(int index) const;
    QString arg(int i, const QString &defaultValue = QString());

    QJSValue throwError(const QString &errorMessage);

    QJSValue newByteArray(const QByteArray &bytes);

    QByteArray fromString(const QString &value) const;

    QVariant toVariant(const QJSValue &value);

    bool toInt(const QJSValue &value, int *number) const;

    QVariantMap toDataMap(const QJSValue &value) const;

    QJSValue fromDataMap(const QVariantMap &dataMap) const;

    QByteArray makeByteArray(const QJSValue &value) const;

    bool toItemData(const QJSValue &value, const QString &mime, QVariantMap *data) const;

    QString getFileName(const QString &fileName) const;

    QJSEngine *engine() const { return m_engine; }

    QJSValue call(QJSValue *function, const QJSValueList &arguments);

    QJSValue eval(const QString &script, const QString &fileName);

    bool hasUncaughtException() const { return m_hasUncaughtException; }
    QString uncaughtExceptionText() const { return m_uncaughtException.toString(); }

public slots:
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

    void filter();

    void ignore();

    QJSValue clipboard();
    QJSValue selection();
    QJSValue hasClipboardFormat();
    QJSValue hasSelectionFormat();
    QJSValue isClipboard();
    QJSValue copy();
    QJSValue copySelection();
    void paste();

    QJSValue tab();
    QJSValue removeTab();
    QJSValue removetab() { return removeTab(); }
    QJSValue renameTab();
    QJSValue renametab() { return renameTab(); }
    QJSValue tabIcon();
    QJSValue tabicon() { return tabIcon(); }

    QJSValue length();
    QJSValue size() { return length(); }
    QJSValue count() { return length(); }

    void select();
    QJSValue next();
    QJSValue previous();
    QJSValue add();
    QJSValue insert();
    QJSValue remove();
    void edit();

    QJSValue read();
    void write();
    void change();
    void separator();

    void action();
    void popup();
    QJSValue notification();

    QJSValue exportTab();
    QJSValue exporttab() { return exportTab(); }
    QJSValue importTab();
    QJSValue importtab() { return importTab(); }

    QJSValue importData();
    QJSValue exportData();

    QJSValue config();
    QJSValue toggleConfig();

    QJSValue info();

    QJSValue eval();

    QJSValue source();

    QJSValue currentPath();
    QJSValue currentpath() { return currentPath(); }

    QJSValue str(const QJSValue &value);
    QJSValue input();
    QJSValue toUnicode();
    QJSValue fromUnicode();

    QJSValue dataFormats();
    QJSValue data(const QJSValue &value);
    QJSValue setData();
    QJSValue removeData();
    void print(const QJSValue &value);
    QJSValue abort();
    void fail();

    QJSValue keys();
    QJSValue testSelected();
    void resetTestSession();
    void flush();

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
    QJSValue setItem();
    void setitem() { setItem(); }

    QJSValue toBase64();
    QJSValue tobase64() { return toBase64(); }
    QJSValue fromBase64();
    QJSValue frombase64() { return fromBase64(); }

    QJSValue open();
    QJSValue execute();

    QJSValue currentWindowTitle();

    QJSValue dialog();

    QJSValue settings();

    QJSValue dateString();

    void updateFirst();
    void updateTitle();

    QJSValue commands();
    void setCommands();
    void addCommands();
    QJSValue importCommands();
    QJSValue exportCommands();

    QJSValue networkGet();
    QJSValue networkPost();

    QJSValue env();
    QJSValue setEnv();

    QJSValue sleep();

    // Call scriptable method.
    QVariant call(const QString &method, const QVariantList &arguments);

    QVariantList currentArguments();

    QJSValue screenshot();
    QJSValue screenshotSelect();
    QJSValue screenNames();

    QJSValue queryKeyboardModifiers();

    QJSValue iconColor();

    void onMessageReceived(const QByteArray &bytes, int messageCode);
    void onDisconnected();

signals:
    void sendMessage(const QByteArray &message, int messageCode);
    void dataReceived();
    void finished();

private slots:
    void onExecuteOutput(const QStringList &lines);

private:
    void executeArguments(const QByteArray &bytes);
    QList<int> getRows() const;
    QJSValue copy(QClipboard::Mode mode);
    QJSValue setClipboard(QVariantMap *data, QClipboard::Mode mode);
    QJSValue changeItem(bool create);
    QJSValue nextToClipboard(int where);
    QJSValue screenshot(bool select);
    QJSValue eval(const QString &script);

    QTextCodec *codecFromNameOrNull(const QJSValue &codecName);
    QJSValue availableCodecNamesError();

    bool hasUncaughtException(const QJSValue &value);

    QJSValue inputSeparator() const;
    void setInputSeparator(const QString &separator);

    QString getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QJSEngine *m_engine;
    ScriptableProxy *m_proxy;

    QJSValue m_input;
    bool m_connected = true;

    QString m_actionName;
    QVariantMap m_data;

    int m_skipArguments = 0;

    QJSValue m_self;

    QJSValue m_executeStdoutCallback;

    bool m_hasUncaughtException = false;
    QJSValue m_uncaughtException;
    bool m_aborted = false;
};

#endif // SCRIPTABLE_H

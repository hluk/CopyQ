#pragma once

#include <QObject>
#include <QString>
#include <QScriptable>
#include <QScriptValue>
#include <QVariantList>

class MainWindow;
class ByteArrayClass;
class ClipboardBrowser;
class QLocalSocket;

const QString defaultMime("text/plain");

class Scriptable : public QObject, protected QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString currentTab READ getCurrentTab WRITE setCurrentTab)
    Q_PROPERTY(QString inputSeparator READ getInputSeparator WRITE setInputSeparator)
    Q_PROPERTY(QString currentPath READ getCurrentPath WRITE setCurrentPath)

public:
    Scriptable(MainWindow *wnd, QLocalSocket* client, QObject *parent = NULL);

    void initEngine(QScriptEngine *engine, const QString &currentPath);

    QScriptValue newByteArray(const QByteArray &bytes);

    QString toString(const QScriptValue &value) const;
    bool toInt(const QScriptValue &value, int &number) const;

    /**
     * Return pointer to QByteArray or NULL.
     */
    QByteArray *toByteArray(const QScriptValue &value) const;

    QScriptValue applyRest(int first);

    ClipboardBrowser *currentTab();

    const QString &getCurrentTab() const;
    void setCurrentTab(const QString &tab);

    const QString &getInputSeparator() const;
    void setInputSeparator(const QString &separator);

    const QString &getCurrentPath() const;
    void setCurrentPath(const QString &path);

    QString getFileName(const QString &fileName) const;

    QString arg(int i, const QString &defaultValue = QString());

    void sendMessage(const QByteArray &message, int exitCode);

public slots:
    QScriptValue version();
    QScriptValue help();

    void show();
    void hide();
    void toggle();
    void menu();
    void exit();

    QScriptValue clipboard();
    QScriptValue selection();

    QScriptValue tab();
    void removetab();
    void renametab();

    QScriptValue length();
    QScriptValue size() { return length(); }
    QScriptValue count() { return length(); }

    void select();
    void add();
    void remove();
    void edit();

    QScriptValue read();
    void write();
    QScriptValue separator();

    void action();

    void exporttab();
    void importtab();

    QScriptValue config();

    QScriptValue eval();

    void currentpath();

    QScriptValue str(const QScriptValue &value);
    void print(const QScriptValue &value);

private:
    MainWindow *m_wnd;
    QLocalSocket* m_client;
    ByteArrayClass *m_baClass;
    QString m_currentTab;
    QString m_inputSeparator;
    QString m_currentPath;

    int getTabIndexOrError(const QString &name);
};


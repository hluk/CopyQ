#pragma once

#include <QObject>
#include <QString>
#include <QScriptable>
#include <QScriptValue>
#include <QVariantList>

class MainWindow;
class ByteArrayClass;
class ClipboardBrowser;

const QString defaultMime("text/plain");

class Scriptable : public QObject, protected QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString currentTab READ getCurrentTab WRITE setCurrentTab)
    Q_PROPERTY(QString inputSeparator READ getInputSeparator WRITE setInputSeparator)

public:
    Scriptable(MainWindow *wnd, ByteArrayClass *baClass,
               QObject *parent = NULL);

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

public slots:
    QScriptValue version();
    QScriptValue help(const QString &commandName = QString());

    QScriptValue show();
    void hide();
    QScriptValue toggle();
    QScriptValue menu();
    QScriptValue exit();

    QScriptValue clipboard(const QString &mime = defaultMime);
    QScriptValue selection(const QString &mime = defaultMime);

    QScriptValue tab();
    void removetab(const QString &name);
    void renametab(const QString &name, const QString &newName);

    QScriptValue length();
    QScriptValue size();
    QScriptValue count();

    void select();
    void add();
    void remove();
    void edit();

    QScriptValue read();
    void write();
    QScriptValue separator();

    void action();

    void exportfile(const QString &fileName);
    void importfile(const QString &fileName);

    QScriptValue config(const QString &name = QString(),
                        const QString &value = QString());

private:
    MainWindow *m_wnd;
    ByteArrayClass *m_baClass;
    QString m_currentTab;
    QString m_inputSeparator;

    int getTabIndexOrError(const QString &name);
};


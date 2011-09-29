#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include <QDialog>
#include <QMutex>
#include <QMap>
#include <QApplication>
#include <QIcon>
#include <QSettings>


namespace Ui {
    class ConfigurationManager;
}

class ClipboardModel;
class QAbstractButton;

class ConfigurationManager : public QDialog
{
    Q_OBJECT

public:
    enum Option {
        Interval = 1,
        Callback,
        Formats,
        MaxItems,
        TrayItems,
        Priority,
        Editor,
        ItemHTML,
        ItemCSS,
        CheckClipboard,
        CheckSelection,
        CopyClipboard,
        CopySelection,
        ConfirmExit
    };

    struct Command {
        QRegExp re;
        QString cmd;
        QString sep;
        bool input;
        bool output;
        bool wait;
        QIcon icon;
        QString shortcut;
    };
    typedef QMap<QString, Command> Commands;

    ~ConfigurationManager();

    static ConfigurationManager *instance(QWidget *parent = 0)
    {
        static QMutex mutex;

        if (!m_Instance)
        {
            mutex.lock();
            if (!m_Instance) {
                m_Instance = new ConfigurationManager(parent);
            }
            mutex.unlock();
        }

        return m_Instance;
    }

    static void drop()
    {
        static QMutex mutex;
        mutex.lock();
        delete m_Instance;
        m_Instance = 0;
        mutex.unlock();
    }

    void loadSettings();
    void saveSettings();

    QVariant value(Option opt) const;
    void setValue(Option opt, const QVariant &value);

    void loadItems(ClipboardModel &model);
    void saveItems(const ClipboardModel &model);

    QByteArray windowGeometry( const QString &widget_name = QString(),
                      const QByteArray &geometry = QByteArray() );

    Commands commands() const;
    void addCommand(const QString &name, const Command *cmd, bool enable=true);

    void readStyleSheet();
    void writeStyleSheet();

signals:
    void configurationChanged();

protected:
    void showEvent(QShowEvent *);

private:
    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QString m_datfilename;
    QMap<Option, QString> m_keys;
    QSettings::Format cssFormat;

    explicit ConfigurationManager(QWidget *parent = 0);

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

private slots:
    void on_pushButtonDown_clicked();
    void on_pushButtonUp_clicked();
    void on_tableCommands_itemSelectionChanged();
    void on_pushButtonRemove_clicked();
    void on_pushButtoAdd_clicked();
    void accept();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int);
};

#endif // CONFIGURATIONMANAGER_H

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

struct _Option;
typedef _Option Option;

class ConfigurationManager : public QDialog
{
    Q_OBJECT

public:
    struct Command {
        QRegExp re;
        QString cmd;
        QString sep;
        bool input;
        bool output;
        bool wait;
        bool automatic;
        bool ignore;
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

    QVariant value(const QString &name) const;
    void setValue(const QString &name, const QVariant &value);

    void loadItems(ClipboardModel &model, const QString &id);
    void saveItems(const ClipboardModel &model, const QString &id);
    void removeItems(const QString &id);

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
    QHash<QString, Option> m_options;
    QSettings::Format cssFormat;

    explicit ConfigurationManager(QWidget *parent = 0);

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

    void getKey(QPushButton *button);

private slots:
    void on_pushButtonDown_clicked();
    void on_pushButtonUp_clicked();
    void on_tableCommands_itemSelectionChanged();
    void on_pushButtonRemove_clicked();
    void on_pushButtoAdd_clicked();
    void apply();
    void on_buttonBox_clicked(QAbstractButton* button);
    void onFinished(int result);
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
};

#endif // CONFIGURATIONMANAGER_H

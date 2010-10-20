#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include <QDialog>
#include <QMutex>
#include <QMap>
#include <QApplication>


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
        Callback = 2,
        Formats = 3,
        MaxItems = 4,
        Priority = 5,
        Editor = 6,
        ItemHTML = 7,
        CheckClipboard = 8,
        CheckSelection = 9,
        CopyClipboard = 10,
        CopySelection = 11,
    };

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

    QRect windowRect( const QString &window_name = QString(),
                      const QRect &newrect = QRect() );

    void readStyleSheet();

signals:
    void configurationChanged();

private:
    static ConfigurationManager *m_Instance;
    Ui::ConfigurationManager *ui;
    QString m_datfilename;
    QMap<Option, QString> m_keys;

    explicit ConfigurationManager(QWidget *parent = 0);

    ConfigurationManager(const ConfigurationManager &);
    ConfigurationManager& operator=(const ConfigurationManager &);

private slots:
    void accept();
    void on_buttonBox_clicked(QAbstractButton* button);
};

#endif // CONFIGURATIONMANAGER_H

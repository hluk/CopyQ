#ifndef APP_H
#define APP_H

#include <QApplication>
#include <QObject>

class App : public QObject
{
    Q_OBJECT
public:
    explicit App(int &argc, char **argv);
    void exit(int exit_code=0);
    int exec();

private:
    QApplication m_app;
    int m_exit_code;
    bool m_closed;

signals:

public slots:
    void quit() { m_app.quit(); }
};

#endif // APP_H

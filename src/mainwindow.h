#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>
#include <QTextDocument>
#include <QSystemTrayIcon>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(const QString &css = QString(), QWidget *parent = 0);
        ~MainWindow();
        void center();
        inline bool browseMode() const { return m_browsemode; };
        void writeSettings();
        void readSettings();

    private:
        Ui::MainWindow *ui;
        QSystemTrayIcon *tray;
        bool m_browsemode;

    protected:
        void closeEvent(QCloseEvent *event);
        void keyPressEvent(QKeyEvent *event);

    public slots:
       void handleMessage(const QString& message);
       void enterBrowseMode(bool browsemode = true);

    private slots:
        void trayActivated(QSystemTrayIcon::ActivationReason reason);
        void enterSearchMode(QEvent *event);
};

#endif // MAINWINDOW_H

#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include "app.h"
#include <QLocalSocket>
#include <QClipboard>
#include <QTimer>

#include "client_server.h"

class QMimeData;
class QByteArray;

class ClipboardMonitor : public App
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(int &argc, char **argv);
    ~ClipboardMonitor();

    bool isConnected()
    {
        return m_socket->state() == QLocalSocket::ConnectedState;
    }

    QMimeData *clipboardData() const;
    void checkClipboard();

    void start() { m_timer.start(); }
    void stop() { m_timer.stop(); }
    void setInterval(int msec) { m_timer.setInterval(msec); }

    void setFormats(const QString &list);

    void updateClipboard(const QMimeData &data, bool force = false);
    uint hash(const QMimeData &data);

    void setCheckClipboard(bool enable) {m_checkclip = enable;}
    void setCheckSelection(bool enable) {m_checksel  = enable;}
    void setCopyClipboard(bool enable)  {m_copyclip  = enable;}
    void setCopySelection(bool enable)  {m_copysel   = enable;}

private:
    QTimer m_timer;
    QStringList m_formats;
    uint m_lastClipboard;
    uint m_lastSelection;
    bool m_checkclip, m_copyclip,
         m_checksel, m_copysel;
    QMimeData *m_newdata;
    QLocalSocket *m_socket;

#ifdef Q_WS_X11
    Display *m_dsp;
#endif

    // don't allow rapid access to clipboard
    QTimer m_updatetimer;

    void clipboardChanged(QClipboard::Mode mode, QMimeData *data);

public slots:
    void timeout();

private slots:
    void updateTimeout();
    void readyRead();
};

#endif // CLIPBOARDMONITOR_H

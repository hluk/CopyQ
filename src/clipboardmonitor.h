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

    bool isConnected()
    {
        return m_socket->state() == QLocalSocket::ConnectedState;
    }

    QMimeData *clipboardData() const;

    void setFormats(const QString &list);

    void updateClipboard(QMimeData *data, bool force = false);
    uint hash(const QMimeData &data);

    void setCheckClipboard(bool enable) {m_checkclip = enable;}
    void setCheckSelection(bool enable) {m_checksel  = enable;}
    void setCopyClipboard(bool enable)  {m_copyclip  = enable;}
    void setCopySelection(bool enable)  {m_copysel   = enable;}

private:
    QStringList m_formats;
    QMimeData *m_newdata;
    bool m_checkclip, m_copyclip,
         m_checksel, m_copysel;
    QTimer m_timer;
    uint m_lastHash;
    QLocalSocket *m_socket;

    // don't allow rapid access to clipboard
    QTimer m_updatetimer;

    void clipboardChanged(QClipboard::Mode mode, QMimeData *data);

public slots:
    void checkClipboard(QClipboard::Mode mode);

private slots:
    bool updateSelection(bool check = true);
    void updateTimeout();
    void readyRead();
};

#endif // CLIPBOARDMONITOR_H

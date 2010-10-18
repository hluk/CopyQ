#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include <QWidget>
#include <QClipboard>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QRegExp>

class QMimeData;
class QByteArray;

class ClipboardMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(QWidget *parent);

    QMimeData *clipboardData() const;
    bool checkClipboard(QClipboard::Mode mode);

    void start() { m_timer.start(); }
    void stop() { m_timer.stop(); }
    void setInterval(int msec) { m_timer.setInterval(msec); }

    void updateClipboard(QClipboard::Mode mode, const QMimeData &data);
    uint hash(const QMimeData &data);

private:
    QWidget *m_parent;
    unsigned long m_interval;
    QTimer m_timer;
    QMutex clipboardLock;
    QRegExp m_re_format;
    uint m_lastSelection;

    QMimeData *cloneData(const QMimeData &data, bool filter=false);

public slots:
    void timeout();

signals:
    void clipboardChanged(QClipboard::Mode mode, QMimeData *data);
};

#endif // CLIPBOARDMONITOR_H

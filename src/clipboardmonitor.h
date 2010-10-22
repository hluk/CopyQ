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
    void checkClipboard();

    void start() { m_timer.start(); }
    void stop() { m_timer.stop(); }
    void setInterval(int msec) { m_timer.setInterval(msec); }

    void setFormats(const QString &list);

    void updateClipboard(QClipboard::Mode mode, const QMimeData &data);
    uint hash(const QMimeData &data);

    void setCheckClipboard(bool enable) {m_checkclip = enable;}
    void setCheckSelection(bool enable) {m_checksel  = enable;}
    void setCopyClipboard(bool enable)  {m_copyclip  = enable;}
    void setCopySelection(bool enable)  {m_copysel   = enable;}

private:
    QWidget *m_parent;
    unsigned long m_interval;
    QTimer m_timer;
    QMutex clipboardLock;
    //QRegExp m_re_format;
    QStringList m_formats;
    uint m_lastSelection;
    bool m_checkclip, m_copyclip,
         m_checksel, m_copysel;

    QMimeData *cloneData(const QMimeData &data, bool filter=false);

public slots:
    void timeout();

signals:
    void clipboardChanged(QClipboard::Mode mode, QMimeData *data);
};

#endif // CLIPBOARDMONITOR_H

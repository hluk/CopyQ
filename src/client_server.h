#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H
#include <QString>
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QByteArray>
#include <QWidget>
#include <QLocalServer>
#include <QClipboard>

#if !defined(Q_WS_X11) && !defined(Q_WS_WIN) && !defined(Q_WS_MAC)
#define NO_GLOBAL_SHORTCUTS
#endif

enum LogLevel {
    LogNote,
    LogWarning,
    LogError
};

void log(const QString &text, const LogLevel level = LogNote);
const QMimeData *clipboardData(QClipboard::Mode mode = QClipboard::Clipboard);
void setClipboardData(QMimeData *data, QClipboard::Mode mode);
bool readBytes(QIODevice *socket, qint64 size, QByteArray *bytes);
bool readMessage(QIODevice *socket, QByteArray *msg);
void writeMessage(QIODevice *socket, const QByteArray &msg);
QLocalServer *newServer(const QString &name, QObject *parent=NULL);
QString serverName(const QString &name);
uint hash(const QMimeData &data, const QStringList &formats);
QMimeData *cloneData(const QMimeData &data, const QStringList *formats=NULL);
void raiseWindow(WId wid);

#endif // CLIENT_SERVER_H

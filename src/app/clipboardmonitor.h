/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPBOARDMONITOR_H
#define CLIPBOARDMONITOR_H

#include "app.h"

#include "common/common.h"

#include <QClipboard>
#include <QLocalSocket>
#include <QScopedPointer>
#include <QStringList>

class QMimeData;
class QTimer;
#ifdef COPYQ_WS_X11
class PrivateX11;
#endif

#ifdef Q_OS_MAC
class MacPlatform;
class MacTimer;
#endif

/**
 * Application monitor server.
 *
 * Monitors clipboard and sends new clipboard data to server.
 * Server can send back new data for clipboard.
 *
 * Only the monitor should change the clipboard content.
 *
 * After monitor is executed it needs to be configured by sending special data
 * packet containing configuration.
 */
class ClipboardMonitor : public QObject, public App
{
    Q_OBJECT

public:
    ClipboardMonitor(int &argc, char **argv);

    ~ClipboardMonitor();

    /** Return true only if monitor socket is connected. */
    bool isConnected() const
    {
        return m_socket->state() == QLocalSocket::ConnectedState;
    }

    /** Change clipboard and primary selection content. */
    void updateClipboard(const QVariantMap &data = QVariantMap());

    virtual void exit(int exitCode);

private slots:
    /**
     * Check clipboard or primary selection.
     * @see updateSelection()
     */
    void checkClipboard(QClipboard::Mode mode);

#ifdef COPYQ_WS_X11
    /**
     * Return true if primary selection data can be retrieved.
     *
     * Don't check primary selection data if mouse button or shift key is still
     * pressed (only on X11).
     */
    bool updateSelection(
            bool check = true
            //!< Call checkClipboard(QClipboard::Selection) afterwards.
            );

    /**
     * Synchronize clipboard and X11 primary selection.
     */
    void synchronize();

    /**
     * Reset clipboard if empty.
     */
    void resetClipboard();
#endif

    /** Update clipboard data in reasonably long intervals. */
    void updateTimeout();

    /** Data can be received from monitor. */
    void readyRead();

    /** Server connection closed. */
    void onDisconnected();

    void clipboardTimeout();

private:
    /** Send new clipboard or primary selection data to server. */
    void clipboardChanged(const QVariantMap &data);

    void writeMessage(const QByteArray &msg);

    void log(const QString &text, const LogLevel level);

    QStringList m_formats;
    QVariantMap m_newdata;
    QLocalSocket *m_socket;

    // don't allow rapid access to clipboard
    QTimer *m_updateTimer;
    bool m_needCheckClipboard;

#ifdef COPYQ_WS_X11
    bool m_needCheckSelection;

    // stuff for X11 window system
    PrivateX11* m_x11;
#endif

#ifdef Q_OS_MAC
    long int m_prevChangeCount;
    MacTimer *m_clipboardCheckTimer;
    MacPlatform *m_macPlatform;
#endif
};

#endif // CLIPBOARDMONITOR_H

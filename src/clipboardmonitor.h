/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include <QLocalSocket>
#include <QClipboard>

#include "client_server.h"

class QMimeData;
class QByteArray;
class QTimer;

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
class ClipboardMonitor : public App
{
    Q_OBJECT

public:
    explicit ClipboardMonitor(int &argc, char **argv);

    /** Return true only if monitor socket is connected. */
    bool isConnected()
    {
        return m_socket->state() == QLocalSocket::ConnectedState;
    }

    /** Set MIME clipboard formats that will be send to server. */
    void setFormats(const QString &list);

    /** Toggle clipboard monitoring. */
    void setCheckClipboard(bool enable) {m_checkclip = enable;}
    /** Toggle primary selection monitoring (only on X11). */
    void setCheckSelection(bool enable) {m_checksel  = enable;}
    /** Toggle copying clipboard contents to primary selection (only on X11). */
    void setCopyClipboard(bool enable)  {m_copyclip  = enable;}
    /** Toggle copying primary selection contents to clipboard (only on X11). */
    void setCopySelection(bool enable)  {m_copysel   = enable;}

    /** Change clipboard and primary selection content. */
    void updateClipboard(QMimeData *data, bool force = false);

private:
    QStringList m_formats;
    QMimeData *m_newdata;
    bool m_checkclip, m_copyclip,
         m_checksel, m_copysel;
    QTimer *m_timer;
    uint m_lastHash;
    QLocalSocket *m_socket;

    // don't allow rapid access to clipboard
    QTimer *m_updateTimer;

    /** Send new clipboard or primary selection data to server. */
    void clipboardChanged(QClipboard::Mode mode, QMimeData *data);

public slots:
    /**
     * Check clipboard or primary selection.
     * @see updateSelection()
     */
    void checkClipboard(QClipboard::Mode mode);

private slots:
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

    /** Update clipboard data in reasonably long intervals. */
    void updateTimeout();

    /** Data can be received from monitor. */
    void readyRead();
};

#endif // CLIPBOARDMONITOR_H

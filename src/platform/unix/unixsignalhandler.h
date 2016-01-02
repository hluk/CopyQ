/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#ifndef UNIXSIGNALHANDLER_H
#define UNIXSIGNALHANDLER_H

#include <QObject>
#include <QSocketNotifier>

/**
 * Gracefully exit application on Unix signals SIGHUP, SIGINT and SIGTERM.
 *
 * More info at http://qt-project.org/doc/qt-4.8/unix-signals.html
 */
class UnixSignalHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * Create signal handler object with given @a parent.
     */
    static bool create(QObject *parent);

    /**
     * Catch Unix signal.
     *
     * Since this can be called at any time, Qt code cannot be handled here. For example,
     * this can be called from QString constructor which can easily deadlock the application on
     * a mutex when trying to create new QString from this handler. Also note that creating
     * QSettings recursively can result in resetting application settings.
     */
    static void exitSignalHandler(int);

public slots:
    void handleSignal();

private:
    explicit UnixSignalHandler(QObject *parent);

    enum SignalAction { Write, Read, Count };
    static int signalFd[Count];

    QSocketNotifier m_signalFdNotifier;
};

#endif // UNIXSIGNALHANDLER_H

/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#ifndef APP_H
#define APP_H

class QCoreApplication;
class QString;

/** Application class. */
class App
{
public:
    explicit App(
            QCoreApplication *application,
            const QString &sessionName
            );

    virtual ~App();

    void restoreSettings(bool canModifySettings = false);

    /**
     * Execute application. Returns immediately if exit() was called before.
     * @return Exit code.
     */
    int exec();

    /**
     * Exit application with given exit code.
     */
    virtual void exit(int exitCode=0);

    /**
     * Return true if exit() was called.
     */
    bool wasClosed() const;

    App(const App &) = delete;
    App &operator=(const App &) = delete;

private:
    QCoreApplication *m_app;
    int m_exitCode;
    bool m_started;
    bool m_closed;
};

#endif // APP_H

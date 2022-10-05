// SPDX-License-Identifier: GPL-3.0-or-later

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

    static void installTranslator();

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

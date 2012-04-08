#include "app.h"

App::App(int &argc, char **argv)
    : m_app(argc, argv)
    , m_exit_code(0)
    , m_closed(false)
{
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");
}

int App::exec()
{
    if (m_closed) {
        m_app.processEvents();
        return m_exit_code;
    } else {
        return m_app.exec();
    }
}

void App::exit(int exit_code)
{
    QCoreApplication::exit(exit_code);
    m_exit_code = exit_code;
    m_closed = true;
}

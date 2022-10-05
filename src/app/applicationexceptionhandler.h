// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPLICATIONEXCEPTIONHANDLER_H
#define APPLICATIONEXCEPTIONHANDLER_H

#include <QObject>

#include <exception>

class QCoreApplication;
class QEvent;

void logException(const char *what = nullptr);

namespace detail {

class ApplicationExceptionHandlerBase : public QObject
{
    Q_OBJECT

protected:
    /// Exit application (thread-safe).
    void exit(int exitCode);

private slots:
    void exitSlot(int exitCode);
};

} // namespace detail

template <typename QtApplication>
class ApplicationExceptionHandler : public detail::ApplicationExceptionHandlerBase, public QtApplication
{
public:
    ApplicationExceptionHandler(int &argc, char **argv)
        : QtApplication(argc, argv)
    {
    }

    bool notify(QObject *receiver, QEvent *event) override
    {
        try {
            return QtApplication::notify(receiver, event);
        } catch (const std::exception &e) {
            logException(e.what());
        } catch (...) {
            logException();
        }

        detail::ApplicationExceptionHandlerBase::exit(1);
        return true;
    }
};

#endif // APPLICATIONEXCEPTIONHANDLER_H

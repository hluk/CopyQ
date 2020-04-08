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

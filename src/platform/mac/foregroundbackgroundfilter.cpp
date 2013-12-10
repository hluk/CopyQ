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

#include "foregroundbackgroundfilter.h"

#include "macplatform.h"
#include <common/common.h>
#include <gui/mainwindow.h>

#include <QWindow>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QShowEvent>
#include <QHideEvent>

namespace {
    QPointer<ForegroundBackgroundFilter> globalFilter = 0;

    MainWindow * getMainWindow() {
        foreach (QWidget *win, QApplication::topLevelWidgets()) {
            if (win->objectName() == "MainWindow") {
                return qobject_cast<MainWindow*>(win);
            }
        }

        return NULL;
    }
}

bool ForegroundBackgroundFilter::installFilter()
{
    if (!globalFilter) {
        globalFilter = new ForegroundBackgroundFilter();
        QCoreApplication *app = QCoreApplication::instance();
        globalFilter->setParent(app);
        app->installEventFilter(globalFilter);
    }

    return (globalFilter != 0);
}

ForegroundBackgroundFilter::ForegroundBackgroundFilter() :
    m_macPlatform(new MacPlatform())
    , m_mainWindow(getMainWindow())
{
}

ForegroundBackgroundFilter::~ForegroundBackgroundFilter()
{
}

bool ForegroundBackgroundFilter::eventFilter(QObject *obj, QEvent *ev)
{
    QWindow *window = qobject_cast<QWindow*>(obj);
    Qt::WindowFlags type = Qt::Widget;
    if (window) {
        type = window->type();
    }

    if (window && (type == Qt::Dialog || type == Qt::Window)) {
        if (ev->type() == QEvent::Show) {
            // Don't foreground if we already are
            if (!m_macPlatform->isNormalApp()) {
                if (!m_mainWindow) {
                    m_mainWindow = getMainWindow();
                }
                if (m_mainWindow && obj != m_mainWindow && !m_mainWindow->isVisible()) {
                    // If the main window is not visible, show the main window,
                    // and repost the event. This is a workaround to fix the
                    // fact that OS X breaks if you don't have a window with a
                    // menu. This workaround could potentially cause issues
                    // elsewhere with focus going to the wrong place, etc. but
                    // is better than having the main window broken..
                    log("Showing main window first to get menu", LogNote);
                    ev->accept();
                    m_mainWindow->showWindow();
                    QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 100);
                    QCoreApplication::postEvent(obj, new QShowEvent());
                    return true;
                }

                COPYQ_LOG("Become normal app");
                m_macPlatform->becomeNormalApp();

                // Forcably raise the main window if we just became
                // "normal" OS X ignores all requests for focus before
                // this.
                usleep(100000);
                m_macPlatform->raiseWindow(window->winId());
            }
        } else if (ev->type() == QEvent::Hide) {
            // Don't background if there are any other visible windows..
            bool onlyWindow = true;
            foreach (QWindow *win, QApplication::topLevelWindows()) {
                if (win != window && win->isVisible() &&
                        (win->type() == Qt::Dialog || win->type() == Qt::Window)) {
                    onlyWindow = false;
                    break;
                }
            }

            if (onlyWindow) {
                COPYQ_LOG("Become background app");
                m_macPlatform->becomeBackgroundApp();
            }
        }
    }
    return false;
}

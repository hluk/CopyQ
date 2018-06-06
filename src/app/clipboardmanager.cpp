/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "clipboardmanager.h"

#include "common/action.h"
#include "common/common.h"
#include "common/mimetypes.h"
#include "gui/actionhandler.h"

#include <QEventLoop>
#include <QTimer>

namespace {

class ClipboardAction {
public:
    explicit ClipboardAction(const QString &scriptFunctionName)
        : m_scriptFunctionName(scriptFunctionName)
    {
    }

    void provideClipboard(const QVariantMap &data, ActionHandler *actionHandler)
    {
        m_act = new Action();
        m_act->setCommand(QStringList() << "copyq" << m_scriptFunctionName);
        m_act->setData(data);
        actionHandler->internalAction(m_act.data());
    }

    QPointer<Action> action() const
    {
        return m_act;
    }

private:
    QString m_scriptFunctionName;
    QPointer<Action> m_act;
};

} // namespace

class ClipboardManagerPrivate {
public:
    explicit ClipboardManagerPrivate(ActionHandler *actionHandler)
        : m_actionHandler(actionHandler)
        , m_clipboardAction("provideClipboard")
        , m_selectionAction("provideSelection")
    {
    }

    void setClipboard(const QVariantMap &data, ClipboardMode mode)
    {
        if (mode == ClipboardMode::Clipboard)
            m_clipboardAction.provideClipboard(data, m_actionHandler);
        else
            m_selectionAction.provideClipboard(data, m_actionHandler);
    }

    QPointer<Action> action(ClipboardMode mode) const
    {
        return mode == ClipboardMode::Clipboard
            ? m_clipboardAction.action()
            : m_selectionAction.action();
    }

private:
    ActionHandler *m_actionHandler;
    ClipboardAction m_clipboardAction;
    ClipboardAction m_selectionAction;
};

ClipboardManager::ClipboardManager(ActionHandler *actionHandler)
    : d(new ClipboardManagerPrivate(actionHandler))
{
}

ClipboardManager::~ClipboardManager() = default;

void ClipboardManager::setClipboard(const QVariantMap &data)
{
    setClipboard(data, ClipboardMode::Clipboard);
    setClipboard(data, ClipboardMode::Selection);
}

void ClipboardManager::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
#ifndef HAS_MOUSE_SELECTIONS
        if (mode == ClipboardMode::Selection)
            return;
#endif

    d->setClipboard(data, mode);
}

bool ClipboardManager::waitForClipboardSet()
{
    return waitForClipboardSet(ClipboardMode::Clipboard)
        && waitForClipboardSet(ClipboardMode::Selection);
}

bool ClipboardManager::waitForClipboardSet(ClipboardMode mode)
{
#ifndef HAS_MOUSE_SELECTIONS
    if (mode == ClipboardMode::Selection)
        return true;
#endif

    const auto act = d->action(mode);
    const auto completed = [&act] { return act == nullptr || act->data().isEmpty(); };
    const auto success = [&act] { return act != nullptr && act->data().isEmpty(); };

    if ( completed() )
        return success();

    QEventLoop loop;

    QTimer timerStop;
    timerStop.setInterval(5000);
    QObject::connect( &timerStop, &QTimer::timeout, &loop, &QEventLoop::quit );
    timerStop.start();

    QTimer timerCheck;
    timerCheck.setInterval(0);
    QObject::connect( &timerCheck, &QTimer::timeout, [&]() {
        if ( completed() )
            loop.quit();
    });
    timerCheck.start();

    loop.exec();

    return success();
}

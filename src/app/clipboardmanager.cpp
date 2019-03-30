/*
    Copyright (c) 2019, Lukas Holecek <hluk@email.cz>

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
#include "common/log.h"
#include "common/mimetypes.h"
#include "gui/actionhandler.h"
#include "platform/platformclipboard.h"
#include "platform/platformnativeinterface.h"

#include <QTimer>

class ClipboardManagerPrivate final {
public:
    explicit ClipboardManagerPrivate(ActionHandler *actionHandler)
        : m_actionHandler(actionHandler)
    {
        m_timerProvideClipboard.setInterval(4000);
        m_timerProvideClipboard.setSingleShot(true);
        QObject::connect(
                    &m_timerProvideClipboard, &QTimer::timeout,
                    [this] { provideClipboard(ClipboardMode::Clipboard, "provideClipboard", &m_clipboardData); });

        m_timerProvideSelection.setInterval(4000);
        m_timerProvideSelection.setSingleShot(true);
        QObject::connect(
                    &m_timerProvideSelection, &QTimer::timeout,
                    [this] { provideClipboard(ClipboardMode::Selection, "provideSelection", &m_selectionData); });
    }

    void setClipboard(QVariantMap data, ClipboardMode mode)
    {
        const auto owner = makeClipboardOwnerData();
        data.insert(mimeOwner, owner);

        // Provide clipboard here first to be able to paste quickly.
        createPlatformNativeInterface()->clipboard()->setData(mode, data);

        // Start separate process later to take over clipboard ownership.
        if (mode == ClipboardMode::Clipboard) {
            m_clipboardData = data;
            m_timerProvideClipboard.start();
        } else {
            m_selectionData = data;
            m_timerProvideSelection.start();
        }
    }

private:
    void provideClipboard(ClipboardMode mode, const QString &scriptFunctionName, QVariantMap *data)
    {
        const auto owner = clipboardOwnerData(mode);
        if ( owner != data->value(mimeOwner).toByteArray() )
            return;

        auto action = new Action();
        action->setCommand(QStringList() << "copyq" << scriptFunctionName);
        action->setData(*data);
        data->clear();
        m_actionHandler->internalAction(action);
    }

    ActionHandler *m_actionHandler;
    QTimer m_timerProvideClipboard;
    QTimer m_timerProvideSelection;
    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;
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

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

#include "traymenu.h"

#include "common/contenttype.h"
#include "common/common.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QApplication>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPixmap>

namespace {

bool canActivate(const QAction &action)
{
    return !action.isSeparator() && action.isEnabled();
}

QAction *firstEnabledAction(QMenu *menu)
{
    // First action is active when menu pops up.
    foreach (QAction *action, menu->actions()) {
        if ( canActivate(*action) )
            return action;
    }

    return NULL;
}

QAction *lastEnabledAction(QMenu *menu)
{
    // First action is active when menu pops up.
    QList<QAction *> actions = menu->actions();
    for (int i = actions.size() - 1; i >= 0; --i) {
        if ( canActivate(*actions[i]) )
            return actions[i];
    }

    return NULL;
}

} // namespace

TrayMenu::TrayMenu(QWidget *parent)
    : QMenu(parent)
    , m_clipboardItemActionsSeparator()
    , m_customActionsSeparator()
    , m_clipboardItemActionCount(0)
    , m_omitPaste(false)
    , m_viMode(false)
{
}

void TrayMenu::toggle()
{
    if ( isVisible() ) {
        close();
        return;
    }

    popup(toScreen(QCursor::pos(), width(), height()));

    raise();
    activateWindow();

    if (!isActiveWindow()) {
        QApplication::processEvents();
        PlatformWindowPtr window = createPlatformNativeInterface()->getWindow(winId());
        if (window)
            window->raise();
    }
}

void TrayMenu::addClipboardItemAction(const QModelIndex &index, bool showImages, bool isCurrent)
{
    QAction *act;

    const QVariantMap data = index.data(contentType::data).toMap();
    act = addAction(QString());

    act->setData(index.data(contentType::hash));

    resetSeparators();
    insertAction(m_clipboardItemActionsSeparator, act);

    QString format;

    // Add number key hint.
    if (m_clipboardItemActionCount < 10) {
        format = tr("&%1. %2",
                    "Key hint (number shortcut) for items in tray menu (%1 is number, %2 is item label)")
                .arg(m_clipboardItemActionCount++);
    }

    const QString label = textLabelForData( data, act->font(), format, true );
    act->setText(label);

    // Menu item icon from image.
    if (showImages) {
        const QStringList formats = data.keys();
        const int imageIndex = formats.indexOf( QRegExp("^image/.*") );
        if (imageIndex != -1) {
            const QString &format = formats[imageIndex];
            QPixmap pix;
            pix.loadFromData( data.value(format).toByteArray(), format.toLatin1().data() );
            const int iconSize = smallIconSize();
            int x = 0;
            int y = 0;
            if (pix.width() > pix.height()) {
                pix = pix.scaledToHeight(iconSize);
                x = (pix.width() - iconSize) / 2;
            } else {
                pix = pix.scaledToWidth(iconSize);
                y = (pix.height() - iconSize) / 2;
            }
            pix = pix.copy(x, y, iconSize, iconSize);
            act->setIcon(pix);
        }
    }

    connect(act, SIGNAL(triggered()), this, SLOT(onClipboardItemActionTriggered()));

    if (isCurrent)
        setActiveAction(act);
}

void TrayMenu::addCustomAction(QAction *action)
{
    resetSeparators();
    insertAction(m_customActionsSeparator, action);
}

void TrayMenu::clearAllActions()
{
    clear();
    m_clipboardItemActionCount = 0;
}

void TrayMenu::setActiveFirstEnabledAction()
{
    QAction *action = firstEnabledAction(this);
    if (action != NULL)
        setActiveAction(action);
}

void TrayMenu::setViModeEnabled(bool enabled)
{
    m_viMode = enabled;
}

void TrayMenu::keyPressEvent(QKeyEvent *event)
{
    int k = event->key();
    m_omitPaste = false;

    if (event->modifiers() == Qt::KeypadModifier && Qt::Key_0 <= k && k <= Qt::Key_9) {
        // Allow keypad digit to activate appropriate item in context menu.
        event->setModifiers(Qt::NoModifier);
    } else if ( m_viMode && handleViKey(event, this) ) {
        return;
    } else {
        // Movement in tray menu.
        switch (k) {
        case Qt::Key_PageDown:
        case Qt::Key_End: {
            QAction *action = lastEnabledAction(this);
            if (action != NULL)
                setActiveAction(action);
            break;
        }
        case Qt::Key_PageUp:
        case Qt::Key_Home: {
            QAction *action = firstEnabledAction(this);
            if (action != NULL)
                setActiveAction(action);
            break;
        }
        case Qt::Key_Escape:
            close();
            break;
        }
    }

    QMenu::keyPressEvent(event);
}

void TrayMenu::mousePressEvent(QMouseEvent *event)
{
    m_omitPaste = (event->button() == Qt::RightButton);
    QMenu::mousePressEvent(event);
}

void TrayMenu::resetSeparators()
{
    if ( m_customActionsSeparator.isNull() ) {
        QAction *firstAction = actions().value(0, NULL);
        m_customActionsSeparator = firstAction != NULL ? insertSeparator(firstAction)
                                                       : addSeparator();
    }

    if ( m_clipboardItemActionsSeparator.isNull() )
        m_clipboardItemActionsSeparator = insertSeparator(m_customActionsSeparator);
}

void TrayMenu::onClipboardItemActionTriggered()
{
    QAction *act = qobject_cast<QAction *>(sender());
    Q_ASSERT(act != NULL);

    QVariant actionData = act->data();
    Q_ASSERT( actionData.isValid() );

    uint hash = actionData.toUInt();
    emit clipboardItemActionTriggered(hash, m_omitPaste);
    close();
}

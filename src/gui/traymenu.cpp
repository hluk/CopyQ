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

#include "traymenu.h"

#include "common/contenttype.h"
#include "common/client_server.h"
#include "item/clipboarditem.h"
#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QMimeData>
#include <QPixmap>
#include <QToolTip>

namespace {

const char propertyHasToolTip[] = "CopyQ_has_tooltip";

void removeAllActions(QList<QPointer<QAction> > *actions, QMenu *menu)
{
    foreach (QPointer<QAction> actionPtr, *actions) {
        if ( !actionPtr.isNull() )
            menu->removeAction(actionPtr.data());
    }
    actions->clear();
}

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
    , m_clipboardItemActions()
    , m_customActions()
    , m_timerShowTooltip()
{
    connect( this, SIGNAL(hovered(QAction*)),
             this, SLOT(onActionHovered(QAction*)) );

    m_timerShowTooltip.setSingleShot(true);
    m_timerShowTooltip.setInterval(250);
    connect( &m_timerShowTooltip, SIGNAL(timeout()),
             this, SLOT(updateTooltip()) );
}

void TrayMenu::toggle()
{
    if ( isVisible() ) {
        close();
        return;
    }

    if (isEmpty())
        return;

    // open menu unser cursor
    QPoint pos = QCursor::pos();
    QRect screen = QApplication::desktop()->screenGeometry(pos);
    pos.setX(qMax(0, qMin(screen.right() - width(), pos.x())));
    pos.setY(qMax(0, qMin(screen.bottom() - height(), pos.y())));
    popup(pos);

    raise();
    activateWindow();

    createPlatformNativeInterface()->raiseWindow(winId());
}

void TrayMenu::addClipboardItemAction(const ClipboardItem &item, bool showImages, bool isCurrent)
{
    QAction *act;

    const int i = m_clipboardItemActions.size();

    act = addAction(item.text());
    m_clipboardItemActions.append(act);

    act->setData( QVariant(item.dataHash()) );

    resetSeparators();
    insertAction(m_clipboardItemActionsSeparator, act);

    elideText(act, true);

    // Add number key hint.
    if (i < 10)
        act->setText( act->text().prepend(QString("&%1. ").arg(i)) );

    QString tooltip = item.data(contentType::notes).toString();
    if ( !tooltip.isEmpty() ) {
        act->setToolTip(tooltip);
        act->setProperty(propertyHasToolTip, true);
    }

    // Menu item icon from image.
    if (showImages) {
        QStringList formats = item.data(contentType::formats).toStringList();
        int i = 0;
        for ( ; i < formats.size(); ++i ) {
            if (formats[i].startsWith("image/"))
                break;
        }
        if (i < formats.size()) {
            QString &format = formats[i];
            QPixmap pix;
            pix.loadFromData( item.data()->data(format), format.toLatin1().data() );
            const int iconSize = 24;
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
    m_customActions.append(action);
    resetSeparators();
    insertAction(m_customActionsSeparator, action);
}

void TrayMenu::clearClipboardItemActions()
{
    removeAllActions(&m_clipboardItemActions, this);
}

void TrayMenu::clearCustomActions()
{
    removeAllActions(&m_customActions, this);
}

void TrayMenu::setActiveFirstEnabledAction()
{
    QAction *action = firstEnabledAction(this);
    if (action != NULL)
        setActiveAction(action);
}

void TrayMenu::keyPressEvent(QKeyEvent *event)
{
    int k = event->key();

    if (event->modifiers() == Qt::KeypadModifier && Qt::Key_0 <= k && k <= Qt::Key_9) {
        // Allow keypad digit to activate appropriate item in context menu.
        const int id = k - Qt::Key_0;
        QAction *act = m_clipboardItemActions.value(id).data();
        if (act != NULL) {
            QVariant data = act->data();
            if ( data.canConvert(QVariant::Int) && data.toInt() == id ) {
                act->trigger();
                close();
            }
        }
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
    emit clipboardItemActionTriggered(hash);
    close();
}

void TrayMenu::onActionHovered(QAction *action)
{
    QToolTip::hideText();

    if ( action->property(propertyHasToolTip).toBool() )
        m_timerShowTooltip.start();
    else
        m_timerShowTooltip.stop();
}

void TrayMenu::updateTooltip()
{
    QAction *action = activeAction();
    if ( action == NULL || !action->property(propertyHasToolTip).toBool() )
        return;

    QPoint pos = actionGeometry(action).topRight();
    QToolTip::showText( mapToGlobal(pos), action->toolTip() );
}

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

#include "traymenu.h"

#include "common/contenttype.h"
#include "common/common.h"
#include "common/display.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "common/timer.h"
#include "gui/icons.h"
#include "gui/iconfactory.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPixmap>
#include <QRegularExpression>

namespace {

const char propertyCustomAction[] = "CopyQ_tray_menu_custom_action";
const char propertyClipboardItemAction[] = "CopyQ_tray_menu_clipboard_item";

const QIcon iconClipboard() { return getIcon("clipboard", IconPaste); }

bool canActivate(const QAction &action)
{
    return !action.isSeparator() && action.isEnabled();
}

QAction *firstEnabledAction(QMenu *menu)
{
    // First action is active when menu pops up.
    for (auto action : menu->actions()) {
        if ( canActivate(*action) )
            return action;
    }

    return nullptr;
}

QAction *lastEnabledAction(QMenu *menu)
{
    // First action is active when menu pops up.
    QList<QAction *> actions = menu->actions();
    for (int i = actions.size() - 1; i >= 0; --i) {
        if ( canActivate(*actions[i]) )
            return actions[i];
    }

    return nullptr;
}

} // namespace

TrayMenu::TrayMenu(QWidget *parent)
    : QMenu(parent)
    , m_clipboardItemActionCount(0)
    , m_omitPaste(false)
    , m_viMode(false)
    , m_numberSearch(false)
{
    m_clipboardItemActionsSeparator = addSeparator();
    m_customActionsSeparator = addSeparator();
    initSingleShotTimer( &m_timerUpdateActiveAction, 0, this, &TrayMenu::updateActiveAction );
    setAttribute(Qt::WA_InputMethodEnabled);
}

void TrayMenu::addClipboardItemAction(const QVariantMap &data, bool showImages)
{
    // Show search text at top of the menu.
    if ( m_clipboardItemActionCount == 0 && m_searchText.isEmpty() )
        setSearchMenuItem( m_viMode ? tr("Press '/' to search") : tr("Type to search") );

    QAction *act = addAction(QString());
    act->setProperty(propertyClipboardItemAction, true);

    act->setData(data);

    insertAction(m_clipboardItemActionsSeparator, act);

    QString format;

    // Add number key hint.
    if (m_clipboardItemActionCount < 10) {
        format = tr("&%1. %2",
                    "Key hint (number shortcut) for items in tray menu (%1 is number, %2 is item label)")
                .arg(m_clipboardItemActionCount);
    }

    m_clipboardItemActionCount++;

    const QString label = textLabelForData( data, act->font(), format, true );
    act->setText(label);

    // Menu item icon from image.
    if (showImages) {
        const QStringList formats = data.keys();
        static const QRegularExpression reImage("^image/.*");
        const int imageIndex = formats.indexOf(reImage);
        if (imageIndex != -1) {
            const auto &mime = formats[imageIndex];
            QPixmap pix;
            pix.loadFromData( data.value(mime).toByteArray(), mime.toLatin1().data() );
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

    if ( act->icon().isNull() ) {
        const QString icon = data.value(mimeIcon).toString();
        if ( !icon.isEmpty() ) {
            const QColor color = getDefaultIconColor(*this);
            const QString tag = data.value(COPYQ_MIME_PREFIX "item-tag").toString();
            act->setIcon( iconFromFile(icon, tag, color) );
        }
    }

    connect(act, &QAction::triggered, this, &TrayMenu::onClipboardItemActionTriggered);

    updateActiveAction();
}

void TrayMenu::clearClipboardItems()
{
    clearActionsWithProperty(propertyClipboardItemAction);

    m_clipboardItemActionCount = 0;

    // Show search text at top of the menu.
    if ( !m_searchText.isEmpty() )
        setSearchMenuItem(m_searchText);
}

void TrayMenu::clearCustomActions()
{
    clearActionsWithProperty(propertyCustomAction);
}

void TrayMenu::addCustomAction(QAction *action)
{
    action->setProperty(propertyCustomAction, true);
    insertAction(m_customActionsSeparator, action);
    updateActiveAction();
}

void TrayMenu::clearAllActions()
{
    clear();
    m_clipboardItemActionCount = 0;
    m_searchText.clear();
}

void TrayMenu::setViModeEnabled(bool enabled)
{
    m_viMode = enabled;
}

void TrayMenu::setNumberSearchEnabled(bool enabled)
{
    m_numberSearch = enabled;
}

void TrayMenu::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    m_omitPaste = false;

    if ( m_viMode && m_searchText.isEmpty() && handleViKey(event, this) ) {
        return;
    } else {
        // Movement in tray menu.
        switch (key) {
        case Qt::Key_PageDown:
        case Qt::Key_End: {
            QAction *action = lastEnabledAction(this);
            if (action != nullptr)
                setActiveAction(action);
            break;
        }
        case Qt::Key_PageUp:
        case Qt::Key_Home: {
            QAction *action = firstEnabledAction(this);
            if (action != nullptr)
                setActiveAction(action);
            break;
        }
        case Qt::Key_Escape:
            close();
            break;
        case Qt::Key_Backspace:
            search( m_searchText.left(m_searchText.size() - 1) );
            break;
        case Qt::Key_Delete:
            search(QString());
            break;
        case Qt::Key_Alt:
            return;
        default:
            // Type text for search.
            if ( (m_clipboardItemActionCount > 0 || !m_searchText.isEmpty())
                 && (!m_viMode || !m_searchText.isEmpty() || key == Qt::Key_Slash)
                 && !event->modifiers().testFlag(Qt::AltModifier)
                 && !event->modifiers().testFlag(Qt::ControlModifier) )
            {
                const QString txt = event->text();
                if ( !txt.isEmpty() && txt[0].isPrint() ) {
                    // Activate item at row when number is entered.
                    if ( !m_numberSearch && m_searchText.isEmpty() ) {
                        bool ok;
                        const int row = txt.toInt(&ok);
                        if (ok && row < m_clipboardItemActionCount) {
                            // Allow keypad digit to activate appropriate item in context menu.
                            if (event->modifiers() == Qt::KeypadModifier)
                                event->setModifiers(Qt::NoModifier);
                            break;
                        }
                    }
                    search(m_searchText + txt);
                    return;
                }
            }
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

void TrayMenu::showEvent(QShowEvent *event)
{
    search(QString());

    // If appmenu is used to handle the menu, most events won't be received
    // so search won't work.
    // This shows the search menu item only if show event is received.
    if ( !m_searchAction.isNull() )
        m_searchAction->setVisible(true);

    QMenu::showEvent(event);
}

void TrayMenu::hideEvent(QHideEvent *event)
{
    QMenu::hideEvent(event);

    if ( !m_searchAction.isNull() )
        m_searchAction->setVisible(false);
}

void TrayMenu::actionEvent(QActionEvent *event)
{
    QMenu::actionEvent(event);
    m_timerUpdateActiveAction.start();
}

void TrayMenu::leaveEvent(QEvent *event)
{
    // Omit clearing active action if menu is resizes and mouse pointer leaves menu.
    auto action = activeAction();
    QMenu::leaveEvent(event);
    setActiveAction(action);
}

void TrayMenu::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty())
        search(m_searchText + event->commitString());
    event->ignore();
}

void TrayMenu::clearActionsWithProperty(const char *property)
{
    for ( auto action : actions() ) {
        if ( action->property(property).toBool() ) {
            removeAction(action);
            delete action;
        }
    }
}

void TrayMenu::search(const QString &text)
{
    if (m_searchText == text)
        return;

    m_searchText = text;
    emit searchRequest(m_viMode ? m_searchText.mid(1) : m_searchText);
}

void TrayMenu::markItemInClipboard(const QVariantMap &clipboardData)
{
    const auto text = getTextData(clipboardData);

    for ( auto action : actions() ) {
        const auto actionData = action->data().toMap();
        const auto itemText = getTextData(actionData);
        if ( !itemText.isEmpty() ) {
            const auto hideIcon = itemText != text;
            const auto isIconHidden = action->icon().isNull();
            if ( isIconHidden != hideIcon )
                action->setIcon(hideIcon ? QIcon() : iconClipboard());
        }
    }
}

void TrayMenu::setSearchMenuItem(const QString &text)
{
    if ( m_searchAction.isNull() ) {
        const QIcon icon = getIcon("edit-find", IconSearch);
        m_searchAction = new QAction(icon, text, this);
        m_searchAction->setEnabled(false);
        // Search menu item is hidden by default, see showEvent().
        m_searchAction->setVisible( isVisible() );
        insertAction( actions().value(0), m_searchAction );
    } else {
        m_searchAction->setText(text);
    }
}

void TrayMenu::onClipboardItemActionTriggered()
{
    QAction *act = qobject_cast<QAction *>(sender());
    Q_ASSERT(act != nullptr);

    const auto actionData = act->data().toMap();
    emit clipboardItemActionTriggered(actionData, m_omitPaste);
    close();
}

void TrayMenu::updateActiveAction()
{
    if ( isVisible() && activeAction() != nullptr )
        return;

    const auto action = firstEnabledAction(this);
    if (action != nullptr)
        setActiveAction(action);
}

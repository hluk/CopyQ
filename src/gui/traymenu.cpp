// SPDX-License-Identifier: GPL-3.0-or-later

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

const char propertyTextFormat[] = "CopyQ_text_format";

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
    initSingleShotTimer( &m_timerUpdateActiveAction, 0, this, &TrayMenu::doUpdateActiveAction );
    setAttribute(Qt::WA_InputMethodEnabled);
}

void TrayMenu::updateTextFromData(QAction *act, const QVariantMap &data)
{
    const QString format = act->property(propertyTextFormat).toString();
    const QString label = textLabelForData( data, act->font(), format, true );
    act->setText(label);
}

bool TrayMenu::updateIconFromData(QAction *act, const QVariantMap &data)
{
    if ( !act->parentWidget() )
        return false;

    const QString icon = data.value(mimeIcon).toString();
    const QString tag = data.value(COPYQ_MIME_PREFIX "item-tag").toString();

    if ( icon.isEmpty() && tag.isEmpty() ) {
        const QString colorName = data.value(mimeColor).toString();
        if ( colorName.isEmpty() )
            return false;

        const QColor color(colorName);
        if ( !color.isValid() )
            return false;

        QPixmap pix(16, 16);
        pix.fill(color);
        act->setIcon(pix);
        return true;
    }

    const QColor color = getDefaultIconColor(*act->parentWidget());
    act->setIcon( iconFromFile(icon, tag, color) );
    return true;
}

QAction *TrayMenu::addClipboardItemAction(const QVariantMap &data, bool showImages)
{
    // Show search text at top of the menu.
    if ( m_clipboardItemActionCount == 0 && m_searchText.isEmpty() )
        setSearchMenuItem( m_viMode ? tr("Press '/' to search") : tr("Type to search") );

    QAction *act = addAction(QString());
    m_clipboardActions.append(act);

    act->setData(data);

    insertAction(m_clipboardItemActionsSeparator, act);

    QString format;

    // Add number key hint.
    const int rowNumber = m_clipboardItemActionCount + static_cast<int>(m_rowIndexFromOne);
    if (rowNumber < 10) {
        format = tr("&%1. %2",
                    "Key hint (number shortcut) for items in tray menu (%1 is number, %2 is item label)")
                .arg(rowNumber);
        act->setProperty(propertyTextFormat, format);
    }

    m_clipboardItemActionCount++;

    updateTextFromData(act, data);

    // Menu item icon from image.
    if (!updateIconFromData(act, data) && showImages) {
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

    connect(act, &QAction::triggered, this, &TrayMenu::onClipboardItemActionTriggered);

    return act;
}

void TrayMenu::clearClipboardItems()
{
    const auto actions = m_clipboardActions;
    m_clipboardActions = {};
    for (QAction *action : actions) {
        removeAction(action);
        action->setVisible(false);
        action->setDisabled(true);
        action->setShortcuts({});
        action->deleteLater();
    }

    m_clipboardItemActionCount = 0;

    // Show search text at top of the menu.
    if ( !m_searchText.isEmpty() )
        setSearchMenuItem(m_searchText);
}

void TrayMenu::clearCustomActions()
{
    const auto actions = m_customActions;
    m_customActions = {};
    for (QAction *action : actions) {
        removeAction(action);
        action->setVisible(false);
        action->setDisabled(true);
        action->setShortcuts({});
        action->deleteLater();
    }
}

void TrayMenu::setCustomActions(QList<QAction*> actions)
{
    clearCustomActions();
    m_customActions = actions;
    insertActions(m_customActionsSeparator, actions);
}

void TrayMenu::clearAllActions()
{
    m_clipboardActions = {};
    m_customActions = {};
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
                        const int start = static_cast<int>(m_rowIndexFromOne);
                        if (ok && start <= row && row < start + m_clipboardItemActionCount) {
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

    if ( m_timerUpdateActiveAction.isActive() )
        doUpdateActiveAction();

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
    delayedUpdateActiveAction();
    QMenu::actionEvent(event);
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
        const QIcon icon = getIcon("edit-find", IconMagnifyingGlass);
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

void TrayMenu::delayedUpdateActiveAction()
{
    if ( !isVisible() || activeAction() == nullptr )
        m_timerUpdateActiveAction.start();
}

void TrayMenu::doUpdateActiveAction()
{
    m_timerUpdateActiveAction.stop();
    const auto action = firstEnabledAction(this);
    if (action != nullptr)
        setActiveAction(action);
}

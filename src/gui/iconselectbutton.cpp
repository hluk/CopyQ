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

#include "iconselectbutton.h"

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QAction>
#include <QMenu>

namespace {

int iconForAction(QAction *action)
{
    const QString iconText = action->text();
    return iconText.isEmpty() ? IconFirst - 1 : action->text().at(0).unicode();
}

} // namespace

IconSelectButton::IconSelectButton(QWidget *parent)
    : QToolButton(parent)
    , m_currentIcon(IconFirst - 1)
{
    setPopupMode(QToolButton::InstantPopup);

    IconFactory *factory = ConfigurationManager::instance()->iconFactory();
    if ( factory->isLoaded() ) {
        const QFont &font = factory->iconFont();
        QFontMetrics fm(font);

        setFont( factory->iconFont() );

        QMenu *menu = new QMenu(this);
        menu->addAction( QString() );
        for (ushort i = IconFirst; i <= IconLast; ++i) {
            QChar c(i);
            if ( fm.inFont(c) ) {
                QAction *act = menu->addAction( QString(c) );
                act->setFont(font);
            }
        }
        setMenu(menu);
        connect( menu, SIGNAL(triggered(QAction*)),
                 this, SLOT(onIconChanged(QAction*)) );
    }
}

void IconSelectButton::setCurrentIcon(int icon)
{
    foreach ( QAction *action, menu()->actions() ) {
        if ( iconForAction(action) == icon ) {
            onIconChanged(action);
            return;
        }
    }

    if (m_currentIcon != IconFirst - 1) {
        m_currentIcon = IconFirst - 1;
        setText(QString());
        emit currentIconChanged(m_currentIcon);
    }
}

void IconSelectButton::onIconChanged(QAction *action)
{
    m_currentIcon = iconForAction(action);
    setText(action->text());
    emit currentIconChanged(m_currentIcon);
}

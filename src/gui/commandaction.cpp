/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "commandaction.h"

#include "common/common.h"
#include "gui/clipboardbrowser.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

CommandAction::CommandAction(const Command &command, CommandAction::Type type, ClipboardBrowser *browser)
    : QAction(browser)
    , m_command(command)
    , m_type(type)
    , m_browser(browser)
{
    Q_ASSERT(browser);

    if (m_type == ClipboardCommand) {
        m_command.transform = false;
        m_command.hideWindow = false;
    }

    setText( elideText(m_command.name, browser->font()) );

    IconFactory *iconFactory = ConfigurationManager::instance()->iconFactory();
    setIcon( iconFactory->iconFromFile(m_command.icon) );
    if (m_command.icon.size() == 1)
        setProperty( "CopyQ_icon_id", m_command.icon[0].unicode() );

    connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));

    browser->addAction(this);
}

void CommandAction::onTriggered()
{
    Command command = m_command;
    if ( command.outputTab.isEmpty() )
        command.outputTab = m_browser->tabName();

    QVariantMap dataMap;

    if (m_type == ClipboardCommand) {
        const QMimeData *data = clipboardData();
        if (data == NULL)
            setTextData( &dataMap, m_browser->selectedText() );
        else
            dataMap = cloneData(*data);
    } else {
        dataMap = m_browser->getSelectedItemData();
    }

    emit triggerCommand(command, dataMap, m_type);
}

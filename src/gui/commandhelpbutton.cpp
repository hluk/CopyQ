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

#include "commandhelpbutton.h"

#include "common/common.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "scriptable/commandhelp.h"

#include <QPalette>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

CommandHelpButton::CommandHelpButton(QWidget *parent)
    : QWidget(parent)
    , m_button(new QPushButton(this))
    , m_help(new QTextBrowser(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    const QColor color = getDefaultIconColor(*this, QPalette::Window);
    m_button->setToolTip( tr("Show command help (F1)") );
    m_button->setShortcut(QKeySequence(Qt::Key_F1));

    m_button->setIcon( getIcon("help-faq", IconInfoSign, color, color) );
    const int h = m_button->sizeHint().height();
    m_button->setFixedSize(h, h);
    connect( m_button, SIGNAL(clicked()),
             this, SLOT(toggleHelp()) );

    m_help->hide();
    m_help->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QString help =
        "<html>"

        "<head><style type=\"text/css\">"
        ".args{font-family:monospace}"
        ".example{font-family:monospace; margin: 0 2em 0 2em}"
        ".description{margin: 0 2em 0 2em}"
        "</style></head>"

        "<body>"
            "<p>"
            + tr("Command can contain <b>%1</b> for item text passed as argument and <b>%2</b> to <b>%9</b> for arguments captured by regular expression if defined.")
            + "</p><p>" +
            tr("Use <b>|</b> to chain commands (pass standard output to next command).")
            + "</p>";

    help.append( tr("<p>Following functions can be used in commands such as:</p>") );
    help.append( "<div class='example'>copyq show '&clipboard'</div>" );
    help.append( "<div class='example'>copyq eval 'show(\"&clipboard\")'</div>" );
    help.append( "<div class='example'>copyq: show('&clipboard')</div>" );

    foreach (const CommandHelp &hlp, commandHelp()) {
        if ( !hlp.cmd.isNull() ) {
            help.append( QString("<p><b>%1</b>"
                                 "&nbsp;<span class='args'>%2</span>"
                                 "<div class='description'>%3</div></p>").arg(
                             escapeHtml(hlp.cmd),
                             escapeHtml(hlp.args),
                             escapeHtml(hlp.desc.trimmed())) );
        }
    }

    m_help->setText(help);

    layout->addWidget(m_button);
    layout->addWidget(m_help);
}

void CommandHelpButton::toggleHelp()
{
    m_help->setVisible( !m_help->isVisible() );
}

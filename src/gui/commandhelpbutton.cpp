/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

QString example(const QString &content)
{
    return "<div class='example'><p>" + escapeHtml(content) + "</p></div>";
}

QString help()
{
    QString help =
        "<html>"

        "<head>"
        "<style type=\"text/css\">"
        ".args{font-family:monospace}"
        ".example{font-family:monospace; margin:0 2em 0 2em; background:lightGray; foreground:black}"
        ".description{margin: 0 2em 0 2em}"
        "</style>"
        "</head>"

        "<body>"

            "<p>"
            + escapeHtml( CommandHelpButton::tr(
                              "Command contains list of programs with arguments which will be executed. For example:") )
            + example("copyq add \"1 + 2 = 3\"; copyq show\ncopyq popup \"1 + 2\" \"= 3\"")
            + " "
            + escapeHtml( CommandHelpButton::tr(
                              "Program argument %1 will be substituted for item text, and %2 through %9 for texts captured by regular expression.") )
            + "</p>"
            + "<p>"
            + escapeHtml( CommandHelpButton::tr(
                              "Character %1 can be used to pass standard output to the next program.") )
            .arg("<b>|</b>")
            + "</p>"

            + "<p>"
            + escapeHtml( CommandHelpButton::tr(
                              "Following syntax can be used to pass rest of the command as single parameter.") )
            + example("perl:\nprint(\"1 + 2 = \", 1 + 2);\nprint(\"; 3 * 4 = \", 3 * 4);")
            + escapeHtml( CommandHelpButton::tr(
                              "This gives same output as %1 but is more useful for longer commands.") )
            .arg( example("perl -e 'print(\"1 + 2 = \", 1 + 2); print(\"; 3 * 4 = \", 3 * 4);'") )
            + "</p>"
            ;

    help.append( "<p>" + escapeHtml(
                     CommandHelpButton::tr("Functions listed below can be used as in following commands.")) + "</p>" );
    const QString tabName = CommandHelpButton::tr("&clipboard", "Example tab name");
    help.append( example("copyq show '" + tabName + "'") );
    help.append( example("copyq eval 'show(\"" + tabName + "\")'") );
    help.append( example("copyq: show('" + tabName + "')") );

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

    help.append("</body></html>");

    return help;
}

} // namespace

CommandHelpButton::CommandHelpButton(QWidget *parent)
    : QWidget(parent)
    , m_button(new QPushButton(this))
    , m_help(new QTextBrowser(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_button->setToolTip( tr("Show command help (F1)") );
    m_button->setShortcut(QKeySequence(Qt::Key_F1));

    m_button->setIcon( getIcon("help-faq", IconInfoSign) );
    const int h = m_button->sizeHint().height();
    m_button->setFixedSize(h, h);

    m_button->setCheckable(true);
    connect( m_button, SIGNAL(clicked(bool)),
             this, SLOT(setHelpVisible(bool)) );

    m_help->setTextInteractionFlags(Qt::TextBrowserInteraction);

    layout->addWidget(m_button);
    layout->addWidget(m_help);

    setHelpVisible(false);
}

void CommandHelpButton::setHelpVisible(bool visible)
{
    m_help->setVisible(visible);
    setFixedHeight( visible ? QWIDGETSIZE_MAX : m_button->height() );

    if (visible) {
        m_help->setFocus();
        if ( m_help->document()->isEmpty() )
            m_help->setText(help());
    } else {
        emit hidden();
    }
}

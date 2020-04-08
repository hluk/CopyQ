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

#include "commandhelpbutton.h"

#include "common/display.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"
#include "scriptable/commandhelp.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QPalette>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const auto htmlTemplate = R"(
<html>
    <head>
        <style type="text/css">
            .args{font-family:monospace}
            .example{font-family:monospace; margin:1em}
            .example-box{background:#777; padding-right:1em}
            .example-margin{padding-right:.5em}
            .description{margin: 0 2em 0 2em}
        </style>
    </head>
    <body>
        %1
    </body>
</html>)";

const auto htmlExampleTemplate = R"(
<table class="example"><tr>
<td class="example-box"></td>
<td class="example-margin"></td>
<td>%1</td>
</tr></table>
)";

QString example(const QString &content)
{
    return QString(htmlExampleTemplate)
            .arg( escapeHtml(content) );
}

QString help()
{
    auto help = QString()
            + "<p>"
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

    for (const auto &hlp : commandHelp()) {
        if ( !hlp.cmd.isNull() ) {
            help.append( QString("<p><b>%1</b>"
                                 "&nbsp;<span class='args'>%2</span>"
                                 "<div class='description'>%3</div></p>").arg(
                             escapeHtml(hlp.cmd),
                             escapeHtml(hlp.args),
                             escapeHtml(hlp.desc.trimmed())) );
        }
    }

    return QString(htmlTemplate).arg(help);
}

QVBoxLayout *createLayout(QWidget *parent)
{
    auto layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return layout;
}

} // namespace

CommandHelpButton::CommandHelpButton(QWidget *parent)
    : QWidget(parent)
    , m_button(new QToolButton(this))
    , m_help(nullptr)
{
    m_button->setToolTip( tr("Show command help (F1)") );
    m_button->setShortcut(QKeySequence(Qt::Key_F1));

    const int x = smallIconSize();
    m_button->setIconSize(QSize(x, x));
    m_button->setIcon( getIcon("help-faq", IconInfoCircle) );

    connect( m_button, &QAbstractButton::clicked,
             this, &CommandHelpButton::showHelp );

    QVBoxLayout *layout = createLayout(this);
    layout->addWidget(m_button);
}

void CommandHelpButton::showHelp()
{
    if (!m_help) {
        m_help = new QDialog(this);
        m_help->setObjectName("commandHelpDialog");
        WindowGeometryGuard::create(m_help);

        auto browser = new QTextBrowser(this);
        QVBoxLayout *layout = createLayout(m_help);
        layout->addWidget(browser);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(
                    QDialogButtonBox::Close, Qt::Horizontal, m_help);
        layout->addWidget(buttonBox);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, m_help, &QWidget::hide);

        browser->setText(help());
    }

    m_help->show();
}

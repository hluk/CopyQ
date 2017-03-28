/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/shortcuts.h"
#include "common/textdata.h"
#include "common/version.h"
#include "gui/icons.h"

namespace {

QString helpKeys(const QString &description, const QString &keys)
{
    static bool odd = true;
    odd = !odd;
    return QString("<tr class='%1'><td>%2</td><td class='key'>%3</td></tr>")
            .arg( odd ? "odd" : "" )
            .arg( escapeHtml(description) )
            .arg( escapeHtml(keys) );
}

QString helpUrl(const char *url)
{
    return QString("<a href='%1'>%1</a>").arg(url);
}

QString helpMail(const char *url)
{
    return QString("<a href='mailto:%1'>%1</a>").arg(url);
}

QString helpLink(const QString &name, const QString &link, ushort icon)
{
    return "<tr>"
           "<td class='info'>" + name + "</td>"
           "<td valign='middle' align='center' class='help-icon icon'>" + QString(QChar(icon)) + "</td>"
           "<td>" + link + "</td>"
           "</tr>";
}

QString helpDeveloper(const char *name, const char *mail)
{
    return QString("<div>%1 &nbsp;&nbsp;&nbsp;<span class='info'>%2</span></div>")
            .arg(QString::fromUtf8(name))
            .arg(mail);
}

QString helpLib(const char *name, const QString &description, const QString &copyright, const char *url)
{
    return QString("<p><span class='library'>%1</span>"
                   "&nbsp;&nbsp;&nbsp;<span class='info'>%2</span><br />%3<br />%4</p>")
            .arg(name)
            .arg( escapeHtml(description) )
            .arg(copyright)
            .arg( helpUrl(url) );
}

QString helpSection(const QString &title, ushort icon)
{
    return "<div class='section'>" + escapeHtml(title)
            + "<span class='section-icon icon'>&nbsp;&nbsp;" + QString(QChar(icon)) + "</span></div>";
}

QString helpParagraph(const QString &text)
{
    return "<p>" + escapeHtml(text) + "</p>";
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->textBrowser->setText( aboutPage() );
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

QString AboutDialog::aboutPage()
{
    return
        "<html>"

        "<head><style type='text/css'>"
        "body{font-size:10pt;background:white;color:#333;margin:1.0em}"
        "p,li{margin-left:4ex;white-space:pre-wrap;margin-left:1ex}"
        "a{text-decoration:none}"
        "table{border:0}"
        "td{padding:0.1em}"
        "#title{font-size:26pt;color:#666;white-space:pre;margin-bottom:0.2em}"
        "#subtitle{font-size:16pt;color:#888;white-space:pre;margin-bottom:0.2em}"
        "#version{font-size:12pt}"
        "#copyright{font-size:9pt;color:#666}"
        ".icon{font-family:FontAwesome}"
        ".help-icon{color:#999;padding-left:1em;padding-right:1em}"
        ".section-icon{color:#777}"
        ".section{font-size:16pt}"
        ".library{font-size:12pt}"
        ".info{color:#666}"
        ".odd{background:#def}"
        "#keys{margin-left:4ex}"
        ".key{font-family:monospace;font-size:9pt;padding-left:0.5em}"
        "</style></head>"

        "<body>"

        "<table><tr valign='middle'>"
        "<td><img src=':/images/logo.png' /></td>"
        "<td>"
        "<div id='title'>CopyQ</div>"
        "<div id='subtitle'>" + escapeHtml(tr("Clipboard Manager")) + "</div>"
        "<div id='version'>" + COPYQ_VERSION "</div>"
        "</td>"
        "</tr></table>"

        "<p>"
        "<table class='links'>"
            + helpLink( tr("Author"), QString::fromUtf8("Lukáš Holeček"), IconUser )
            + helpLink( tr("E-mail"), helpMail("hluk@email.cz"), IconEnvelope )
            + helpLink( tr("Web"), helpUrl("https://hluk.github.io/CopyQ/"), IconHome )
            + helpLink( tr("Wiki"), helpUrl("https://github.com/hluk/CopyQ/wiki"), IconBook )
            + helpLink( tr("Donate"), helpUrl("https://www.bountysource.com/teams/copyq"), IconGift )
            +
        "</table>"
        "</p>"

        "<p id='copyright'>Copyright (c) 2009 - 2017</p>"

        "<p></p>"

        + helpSection(tr("Development"), IconPencil)
        + "<p>"
            + helpDeveloper("Adam Batkin", "adam@batkin.net")
            + helpDeveloper("Giacomo Margarito", "giacomomargarito@gmail.com")
            + helpDeveloper("Greg Carp", "grcarpbe@gmail.com")
            + helpDeveloper("Ilya Plenne", "libbkmz.dev@gmail.com")
            + helpDeveloper("Jörg Thalheim", "joerg@higgsboson.tk")
            + helpDeveloper("Kim Jzhone", "jzhone@gmail.com")
            + helpDeveloper("Kos Ivantsov", "kos.ivantsov@gmail.com")
            + helpDeveloper("lightonflux", "lightonflux@znn.info")
            + helpDeveloper("Lukas Holecek", "hluk@email.cz")
            + helpDeveloper("Marjolein Hoekstra", "http://twitter.com/cleverclogs")
            + helpDeveloper("Martin Lepadusch", "mlepadusch@googlemail.com")
            + helpDeveloper("Matt d'Entremont", "mattdentremont@gmail.com")
            + helpDeveloper("Michal Čihař", "michal@cihar.com")
            + helpDeveloper("Patricio M. Ros", "patricioros.dev@gmail.com")
            + helpDeveloper("Robert Orzanna", "robert@orzanna.de>")
            + helpDeveloper("Ryan Wooden", "rygwdn@gmail.com")
            + helpDeveloper("Scott Kostyshak", "skostysh@princeton.edu")
            + helpDeveloper("Sebastian Schuberth", "sschuberth@gmail.com")
            + helpDeveloper("Tomas Nilzon", "tomas.nilzon@telia.com")
            + helpDeveloper("Wilfried Caruel", "wilfried.caruel@gmail.com")
            + helpDeveloper("x2357", "x2357handle@gmail.com")
            +
        "</p>"

            + helpLib("Qt", "",
                      "The Qt Toolkit is Copyright (C) 2016 The Qt Company Ltd. and other contributors.", "https://www.qt.io/")
            + helpLib("Weblate", tr("Free web-based translation management system", "Weblate description"),
                      "Copyright (c) 2012 - 2016 Michal &#268;iha&#345;", "https://weblate.org")
            + helpLib("Font Awesome", tr("Iconic font used in the application", "Font Awesome description"),
                      "Created & Maintained by Dave Gandy", "https://fortawesome.github.io/Font-Awesome/")
            + helpLib("LibQxt", "",
                      "Copyright (c) 2006 - 2011, the LibQxt project", "http://libqxt.org")
            + helpLib("Solarized", tr("Color palette used for themes", "Solarized palette/themes description"),
                      "Copyright (c) 2011 Ethan Schoonover", "http://ethanschoonover.com/solarized")

        // keyboard title
        + helpSection(tr("Keyboard"), IconKeyboard)

        + helpParagraph(tr("Application shortcuts can be changed in Preferences dialog."))

#ifndef NO_GLOBAL_SHORTCUTS
        + helpParagraph(tr("Global shortcuts (system-wide shortcuts) can be set in Command dialog (default shortcut is F6)."))
#endif

        + helpParagraph(tr("Type any text to search the clipboard history."))

        // keyboard table
        + "<p><table id='keys'>"
            + helpKeys( tr("Item list navigation"), tr("Up/Down, Page Up/Down, Home/End") )
            + helpKeys( tr("Tab navigation"),
                        tr("Left, Right, %1, %2", "Keys for tab navigation (%1, %2 are the standard keys).")
                        .arg(QKeySequence(QKeySequence::NextChild).toString(QKeySequence::NativeText))
                        .arg(QKeySequence(QKeySequence::PreviousChild).toString(QKeySequence::NativeText))
                        )
            + helpKeys( tr("Move selected items"), tr("Ctrl+Up/Down, Ctrl+Home/End") )
            + helpKeys( tr("Reset search or hide window"), tr("Escape") )
            + helpKeys( tr("Delete item"), shortcutToRemove() )
            + helpKeys( tr("Put selected items into clipboard"), tr("Enter") )
            + helpKeys( tr("Change item display format"), tr("Ctrl+Left/Right") )
            + helpKeys( tr("Edit Item"), tr("F2") )
            +
        "</table></p>"

        + "<p></p>"

        "</body></html>";
}

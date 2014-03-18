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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/common.h"
#include "configurationmanager.h"

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
    return QString("<a href='%2'>%2</a>").arg(url);
}

QString helpMail(const char *url)
{
    return QString("<a href='mailto:%1'>%1</a>").arg(url);
}

QString helpLink(const QString &name, const QString &link)
{
    return QString("<tr><td class='h3'>%1</td><td class='link'>%2</td></tr>").arg(name, link);
}

QString helpDeveloper(const char *name, const char *mail)
{
    return QString("<div>%1 &nbsp;&nbsp;&nbsp;<span class='h3'>%2</span></div>")
            .arg(name)
            .arg(mail);
}

QString helpLib(const char *name, const QString &description, const QString &copyright, const char *url)
{
    return QString("<p class=\"ppp\"><span class='h2x'>%1</span>"
                   "&nbsp;&nbsp;&nbsp;<span class='h3'>%2</span><br />%3<br />%4</p>")
            .arg(name)
            .arg( escapeHtml(description) )
            .arg(copyright)
            .arg( helpUrl(url) );
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->textBrowser->setText( aboutPage() );
    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));

    ConfigurationManager::instance()->loadGeometry(this);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

QString AboutDialog::aboutPage()
{
    return
        "<html>"

        // CSS
        "<head><style type=\"text/css\">"
        "body{font-size:10pt;background:white;color:#333;margin:1.5em}"
        "p, li{white-space:pre-wrap;margin-left:1ex}"
        "a{text-decoration:none}"
        ".h1{font-size:26pt;color:#666;white-space:pre}"
        ".h1x{font-size:16pt;color:#888;white-space:pre}"
        ".h2{width:100%;font-size:16pt;margin-left:1ex;margin-top:0.2em}"
        ".h2x{font-size:12pt}"
        ".h3{font-size:9pt;color:#666}"
        ".links td{padding:0}"
        ".link{font-size:9pt}"
        ".pp{margin-left:4ex}"
        ".ppp{margin-left:4ex;font-size:9pt}"
        "table{border:0}"
        ".odd {background:#def}"
        "td{padding:0.1em}"
        "#keys{margin-left:4ex}"
        ".key{font-family:monospace;font-size:9pt;padding-left:0.5em}"
        ".logo{float:left}"
        ".info{font-size:9pt}"
        "</style></head>"

        "<body>"

        // logo
        "<img class='logo' src=':/images/logo.svg' />"

        // title
        "<div class='h1'>CopyQ</div>"
        // subtitle
        "<div class=\"h1x\">" + escapeHtml(tr("Clipboard Manager"))
            + " v" COPYQ_VERSION "</div>"

        "<p>"
        "<table class='links'>"
            + helpLink( tr("Author"), "Lukas Holecek" )
            + helpLink( tr("E-mail"), helpMail("hluk@email.cz") )
            + helpLink( tr("Web"), helpUrl("http://hluk.github.io/CopyQ/") )
            + helpLink( tr("Wiki"), helpUrl("https://sourceforge.net/p/copyq/wiki/Home/") )
            +
        "</table>"
        "</p>"

        // copyright
        "<p class='info'>Copyright (c) 2009 - 2014</p>"

        "<p></p>"

        "<div class='h2'>" + tr("Development") + "</div>"
        "<p class=\"pp\">"
            // developers
            + helpDeveloper("Adam Batkin", "adam@batkin.net")
            + helpDeveloper("Ilya Plenne", "libbkmz.dev@gmail.com")
            + helpDeveloper("J&#246;rg Thalheim", "joerg@higgsboson.tk")
            + helpDeveloper("lightonflux", "lightonflux@znn.info")
            + helpDeveloper("Martin Lepadusch", "mlepadusch@googlemail.com")
            + helpDeveloper("Matt d'Entremont", "mattdentremont@gmail.com")
            + helpDeveloper("Patricio M. Ros", "patricioros.dev@gmail.com")
            + helpDeveloper("Ryan Wooden", "rygwdn@gmail.com")
            + helpDeveloper("Scott Kostyshak", "skostysh@princeton.edu")
            + helpDeveloper("Sebastian Schuberth", "sschuberth@gmail.com")
            + helpDeveloper("Tomas Nilzon", "tomas@tonil.eu")
            +
        "</p>"

            // libraries
            + helpLib("Qt", tr("Library used in the application"),
                      "Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies)", "http://qt.digia.com/")
            + helpLib("LibQxt", tr("Library used in the application"),
                      "Copyright (c) 2006 - 2011, the LibQxt project", "http://libqxt.org")
            + helpLib("Font Awesome", tr("Iconic font used in the application"),
                      "Created & Maintained by Dave Gandy", "http://fortawesome.github.com/Font-Awesome")
            + helpLib("Solarized", tr("Color palette used for themes"),
                      "Copyright (c) 2011 Ethan Schoonover", "http://ethanschoonover.com/solarized")
            +

        // keyboard title
        "<div class='h2'>" + escapeHtml(tr("Keyboard")) + "</div>"
        "<p class=\"pp\">" + escapeHtml(tr("Type any text to search the clipboard history.")) + "</p>"

        // keyboard table
        "<p><table id=\"keys\">"
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

        "<p></p>"

        "</body></html>";
}

void AboutDialog::onFinished(int)
{
    ConfigurationManager::instance()->saveGeometry(this);
}

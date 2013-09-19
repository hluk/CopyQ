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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/client_server.h"
#include "configurationmanager.h"

static QString aboutPage()
{
    return
        "<html>"

        // CSS
        "<head><style type=\"text/css\">"
        "body{font-size:10pt;background-color:white;color:black}"
        "p, li{white-space:pre-wrap;margin-left:1ex}"
        ".h1{font-size:20pt;color:#444}"
        ".h1x{font-size:12pt;color:#222;}"
        ".h2{width:100%;font-size:16pt;color:#333;margin-left:1ex;margin-top:0.2em}"
        ".h2x{font-size:12pt;color:#222;}"
        ".h3{font-size:9pt;color:#444}"
        ".pp{margin-left:4ex}"
        ".ppp{margin-left:4ex;font-size:9pt}"
        "table{border:0}"
        ".odd {background-color:#def}"
        "td{padding:0.1em}"
        "#keys{margin-left:4ex}"
        ".key{color:#333;font-family:monospace;font-size:9pt;padding-left:0.5em}"
        "</style></head>"

        "<body>"

        "<p><table><tr>"

        // logo
        "<td><img src=\":/images/logo.svg\" /></td>"

        "<td>"

        // title
        "<div class='h1'>CopyQ</div>"
        // subtitle
        "<div class=\"h1x\">" + escapeHtml(AboutDialog::tr("Clipboard Manager"))
            + " v" COPYQ_VERSION "</div>"

        "<p></p>"

        "<p><table>"

        // copyright
        "<tr><td colspan=\"2\">Copyright (c) 2009 - 2013</td></tr>"
        // author
        "<tr><td colspan=\"2\">Lukas Holecek</td></tr>"
        // e-mail
        "<tr>"
        "<td class='h3'>" + escapeHtml(AboutDialog::tr("E-mail")) + "</td>"
        "<td><a href=\"mailto:hluk@email.cz\">hluk@email.cz</a></td>"
        "</tr>"
        // web
        "<tr>"
        "<td class='h3'>" + escapeHtml(AboutDialog::tr("Web")) + "</td>"
        "<td><a href=\"https://sourceforge.net/projects/copyq/\">https://sourceforge.net/projects/copyq/</a></td>"
        "</tr>"
        "<tr>"
        "<td></td>"
        "<td><a href=\"http://github.com/hluk/copyq\">github.com/hluk/copyq</a></td>"
        "</tr>"
        // wiki
        "<tr>"
        "<td class='h3'>" + escapeHtml(AboutDialog::tr("Wiki")) + "</td>"
        "<td><a href=\"https://sourceforge.net/p/copyq/wiki/Home/\">https://sourceforge.net/p/copyq/wiki/Home/</a></td>"
        "</tr>"

        "</table></p>"

        "</td>"

        "</tr></table></p>"

        // developers
        "<div class='h2'>" + AboutDialog::tr("Development") + "</div>"
        "<p class=\"pp\">"
        "Adam Batkin (<a href=\"mailto:adam@batkin.net\">adam@batkin.net</a>)<br />"
        "Ilya Plenne (<a href=\"mailto:libbkmz.dev@gmail.com\">libbkmz.dev@gmail.com</a>)<br />"
        "J&#246;rg Thalheim (<a href=\"mailto:joerg@higgsboson.tk\">joerg@higgsboson.tk</a>)<br />"
        "lightonflux (<a href=\"mailto:lightonflux@znn.info\">lightonflux@znn.info</a>)<br />"
        "Patricio M. Ros (<a href=\"mailto:patricioros.dev@gmail.com\">patricioros.dev@gmail.com</a>)<br />"
        "Scott Kostyshak (<a href=\"mailto:skostysh@princeton.edu\">skostysh@princeton.edu</a>)<br />"
        "Sebastian Schuberth (<a href=\"mailto:sschuberth@gmail.com\">sschuberth@gmail.com</a>)<br />"
        "Tomas Nilzon (<a href=\"tomas@nilzon.eu\">tomas@nilzon.eu</a>)"
        "</p>"

        // libraries
        "<p class=\"ppp\"><span class='h2x'>Qt</span> (" + escapeHtml(AboutDialog::tr("Library used in the application")) + ")<br />"
        "Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies) (<a href=\"http://qt.digia.com/\">http://qt.digia.com/</a>).<br />"
        "All rights reserved.</p>"

        "<p class=\"ppp\"><span class='h2x'>LibQxt</span> (" + escapeHtml(AboutDialog::tr("Library used in the application")) + ")<br />"
        "Copyright (c) 2006 - 2011, the LibQxt project (<a href=\"http://libqxt.org/\">http://libqxt.org</a>).<br />"
        "All rights reserved.</p>"

        "<p class=\"ppp\"><span class='h2x'>Font Awesome</span> (" + escapeHtml(AboutDialog::tr("Iconic font used in the application")) + ")<br />"
        "Created & Maintained by Dave Gandy (<a href=\"http://fortawesome.github.com/Font-Awesome/\">http://fortawesome.github.com/Font-Awesome/</a>).</p>"

        "<p class=\"ppp\"><span class='h2x'>Solarized</span> (" + escapeHtml(AboutDialog::tr("Color palette used for themes")) + ")<br />"
        "Copyright (c) 2011 Ethan Schoonover (<a href=\"http://ethanschoonover.com/solarized\">http://ethanschoonover.com/solarized</a>).</p>"

        // keyboard title
        "<div class='h2'>" + escapeHtml(AboutDialog::tr("Keyboard")) + "</div>"
        "<p class=\"pp\">" + escapeHtml(AboutDialog::tr("Type any text to search the clipboard history.")) + "</p>"

        // keyboard table
        "<p><table id=\"keys\">"
        "<tr class=\"odd\">"
        "<td>" + escapeHtml(AboutDialog::tr("Item list navigation")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Up/Down, Page Up/Down, Home/End")) + "</td>"
        "</tr>"
        "<tr><td>" + escapeHtml(AboutDialog::tr("Tab navigation")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Left, Right, Tab, Shift+Tab")) + "</td>"
        "</tr>"
        "<tr class=\"odd\">"
        "<td>" + escapeHtml(AboutDialog::tr("Move selected items")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Ctrl+Up/Down, Ctrl+Home/End")) + "</td>"
        "</tr>"
        "<tr><td>" + escapeHtml(AboutDialog::tr("Reset search or hide window")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Escape")) + "</td>"
        "</tr>"
        "<tr class=\"odd\">"
        "<td>" + escapeHtml(AboutDialog::tr("Delete item")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Delete")) + "</td>"
        "</tr>"
        "<tr><td>" + escapeHtml(AboutDialog::tr("Put selected items into clipboard")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Enter")) + "</td>"
        "</tr>"
        "<tr class=\"odd\">"
        "<td>" + escapeHtml(AboutDialog::tr("Change item display format")) + "</td>"
        "<td class=\"key\">" + escapeHtml(AboutDialog::tr("Ctrl+Left/Right")) + "</td>"
        "</tr>"
        "</table></p>"

        "<p></p>"

        "</body></html>";
}

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

void AboutDialog::onFinished(int)
{
    ConfigurationManager::instance()->saveGeometry(this);
}

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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/shortcuts.h"
#include "common/textdata.h"
#include "common/version.h"
#include "gui/icons.h"
#include "gui/iconfont.h"

namespace {

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
            .arg(QString::fromUtf8(name), mail);
}

QString helpLib(const char *name, const QString &copyright, const char *url)
{
    return QString("<p><span class='library'>%1</span>"
                   "&nbsp;&nbsp;&nbsp;<br />"
                   "<span class='copyright'>%2</span><br />"
                   "%3</p>")
            .arg( name, copyright, helpUrl(url) );
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
        "a{text-decoration:none;color:#4f9a6b}"
        "table{border:0}"
        "td{padding:0.1em}"
        "#title{font-size:26pt;color:#666;white-space:pre;margin-bottom:0.2em}"
        "#subtitle{font-size:16pt;color:#4f9a6b;white-space:pre;margin-bottom:0.2em}"
        "#version{font-size:12pt}"
        ".copyright{font-size:9pt;color:#666}"
        ".icon{font-family:" + iconFontFamily() + "}"
        ".help-icon{color:#5faa7b;padding-left:1em;padding-right:1em}"
        ".library{font-size:12pt}"
        ".info{color:#666}"
        "</style></head>"

        "<body>"

        "<table><tr valign='middle'>"
        "<td><img src=':/images/logo.png' width='128' /></td>"
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
            + helpLink( tr("Donate"), helpUrl("https://liberapay.com/CopyQ/"), IconGift )
            +
        "</table>"
        "</p>"

        "<p class='copyright'>Copyright (c) 2009 - 2020</p>"

        "<p></p>"

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
            + helpDeveloper("Robert Orzanna", "robert@orzanna.de")
            + helpDeveloper("Ryan Wooden", "rygwdn@gmail.com")
            + helpDeveloper("Scott Kostyshak", "skostysh@princeton.edu")
            + helpDeveloper("Sebastian Schuberth", "sschuberth@gmail.com")
            + helpDeveloper("Tomas Nilzon", "tomas.nilzon@telia.com")
            + helpDeveloper("Wilfried Caruel", "wilfried.caruel@gmail.com")
            + helpDeveloper("x2357", "x2357handle@gmail.com")
            +
        "</p>"

            + helpLib("Qt Toolkit",
                      "Copyright (c) 2016 The Qt Company Ltd. and other contributors", "https://www.qt.io/")
            + helpLib("Weblate",
                      "Copyright (c) 2012 - 2017 Michal &#268;iha&#345;", "https://weblate.org")
            + helpLib("Font Awesome",
                      "Copyright (c) 2017 Fonticons, Inc.", "https://fontawesome.com")
            + helpLib("LibQxt",
                      "Copyright (c) 2006 - 2011, the LibQxt project", "http://libqxt.org")
            + helpLib("Solarized",
                      "Copyright (c) 2011 Ethan Schoonover", "http://ethanschoonover.com/solarized")

        + "<p></p>"

        "</body></html>";
}

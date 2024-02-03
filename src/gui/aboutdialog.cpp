// SPDX-License-Identifier: GPL-3.0-or-later

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/textdata.h"
#include "common/version.h"
#include "gui/icons.h"
#include "gui/iconfont.h"

namespace {

QString helpUrl(const char *url)
{
    return QString::fromLatin1("<a href='%1'>%1</a>").arg(url);
}

QString helpMail(const char *url)
{
    return QString::fromLatin1("<a href='mailto:%1'>%1</a>").arg(url);
}

QString helpLink(const QString &name, const QString &link, ushort icon)
{
    return "<tr>"
           "<td class='info'>" + name + "</td>"
           "<td valign='middle' align='center' class='help-icon icon'>"
           + QString("&#%1;").arg(icon) + "</td>"
           "<td>" + link + "</td>"
           "</tr>";
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
        ".icon{font-family: \"" + iconFontFamily() + "\"}"
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
        "<div id='version'>" + versionString + "</div>"
        "</td>"
        "</tr></table>"

        "<p>"
        "<table class='links'>"
            + helpLink( tr("Author"), QString::fromUtf8("Lukáš Holeček"), IconUser )
            + helpLink( tr("E-mail"), helpMail("copyq@googlegroups.com"), IconEnvelope )
            + helpLink( tr("Web"), helpUrl("https://hluk.github.io/CopyQ/"), IconHouse )
            + helpLink( tr("Donate"), helpUrl("https://liberapay.com/CopyQ/"), IconGift )
            +
        "</table>"
        "</p>"

        "<p class='copyright'>Copyright (c) 2009 - 2024</p>"

        "<p></p>"

        + "<p>"
            + helpUrl("https://github.com/hluk/CopyQ/graphs/contributors")
            +
        "</p>"

            + helpLib("Qt Toolkit",
                      "Copyright (c) The Qt Company Ltd. and other contributors",
                      "https://www.qt.io/")
            + helpLib("KDE Frameworks",
                      "Copyright (c) KDE Community",
                      "https://develop.kde.org/products/frameworks/")
            + helpLib("Snoretoast",
                      "Copyright (c) Hannah von Reth",
                      "https://invent.kde.org/libraries/snoretoast")
            + helpLib("Weblate",
                      "Copyright (c) Michal &#268;iha&#345;", "https://weblate.org")
            + helpLib("Font Awesome",
                      "Copyright (c) Fonticons, Inc.", "https://fontawesome.com")
            + helpLib("LibQxt",
                      "Copyright (c), the LibQxt project", "http://libqxt.org")
            + helpLib("Solarized",
                      "Copyright (c) Ethan Schoonover", "https://ethanschoonover.com/solarized")

        + "<p></p>"

        "</body></html>";
}

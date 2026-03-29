// SPDX-License-Identifier: GPL-3.0-or-later

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "common/textdata.h"
#include "common/diagnostics.h"
#include "common/version.h"
#include "gui/icons.h"
#include "gui/iconfont.h"
#include "gui/theme.h"

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
    return QString("<span class='library'>%1</span>"
                   "&nbsp;&nbsp;&nbsp;<br />"
                   "<span class='copyright'>\u00a9 %2</span><br />"
                   "%3")
            .arg( name, copyright, helpUrl(url) );
}

QString helpLibColumns(int columns, const QStringList &libs)
{
    const QString width = QString::number(100 / columns) + QLatin1Char('%');
    QString html = QStringLiteral("<p><table width='100%'>");
    for (int i = 0; i < libs.size(); i += columns) {
        html += QLatin1String("<tr>");
        for (int j = 0; j < columns; ++j) {
            html += QLatin1String("<td width='") + width + QLatin1String("' valign='top'>");
            if (i + j < libs.size())
                html += libs[i + j];
            html += QLatin1String("</td>");
        }
        html += QLatin1String("</tr>");
    }
    html += QLatin1String("</table></p>");
    return html;
}

QString diagnosticSection()
{
    return "<br/><h3><a name='diagnostics'>Diagnostic Information</a></h3>"
           "<pre>" + escapeHtml("CopyQ " + QString(versionString) + "\n" + diagnosticText()) + "</pre>";
}

} // namespace

AboutDialog::AboutDialog(const Theme &theme, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->textBrowser->setText( aboutPage(theme) );
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

QString AboutDialog::aboutPage(const Theme &theme)
{
    const auto bg = theme.color("bg").name();
    const auto fg = theme.color("fg").name();
    const auto link = theme.color("num_fg").name();
    return
        "<html>"

        "<head><style type='text/css'>"
        "body{font-size:10pt;background:" + bg + ";color:" + fg + ";margin:1.0em}"
        "p,li{margin-left:4ex;white-space:pre-wrap;margin-left:1ex}"
        "a{text-decoration:none;color:" + link + "}"
        "table{border:0}"
        "td{padding:0.1em}"
        "#title{font-size:26pt;white-space:pre;margin-bottom:0.2em}"
        "#subtitle{font-size:16pt;color:" + link + ";white-space:pre;margin-bottom:0.2em}"
        "#version{font-size:12pt}"
        ".copyright{font-size:9pt}"
        ".icon{font-family: \"" + iconFontFamily() + "\"}"
        ".help-icon{color:" + link + ";padding-left:1em;padding-right:1em}"
        ".library{font-size:12pt}"
        "h3{font-size:14pt}"
        "pre{font-size:9pt;white-space:pre-wrap;margin:0.5em 0}"
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

        "<p><a href='#diagnostics'><span class='icon'>&#" + QString::number(IconCircleInfo) + ";</span> Diagnostic Information</a></p>"

        "<p class='copyright'>&copy; 2009 - 2026</p>"

        + "<p>"
            + helpUrl("https://github.com/hluk/CopyQ/graphs/contributors")
            +
        "</p>"

        + helpLibColumns(2, {
            helpLib("Qt Framework",
                    "The Qt Company and other contributors",
                    "https://www.qt.io/development/qt-framework"),
            helpLib("KDE Frameworks",
                    "KDE Community",
                    "https://develop.kde.org/products/frameworks/"),
            helpLib("Qt Cryptographic Architecture",
                    "Justin Karneges, Brad Hards, Ivan Romanov",
                    "https://invent.kde.org/libraries/qca"),
            helpLib("QtKeychain",
                    "Frank Osterfeld",
                    "https://github.com/frankosterfeld/qtkeychain"),
            helpLib("LibQxt",
                    "the LibQxt project", "https://bitbucket.org/libqxt/libqxt/wiki/Home"),
            helpLib("Weblate",
                    "Michal &#268;iha&#345;", "https://weblate.org"),
            helpLib("Font Awesome",
                    "Fonticons, Inc.", "https://fontawesome.com"),
            helpLib("Solarized",
                    "Ethan Schoonover", "https://ethanschoonover.com/solarized"),
            helpLib("Snoretoast",
                    "Hannah von Reth",
                    "https://invent.kde.org/libraries/snoretoast"),
            helpLib("miniaudio",
                    "David Reid",
                    "https://miniaud.io/"),
        })

        + diagnosticSection()

        + "</body></html>";
}

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

#include "theme.h"

#include "ui_configtabappearance.h"

#include "gui/iconfont.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QListView>
#include <QSettings>

#include <cmath>

namespace {

int normalizeColorValue(float value)
{
    return qBound( 0, static_cast<int>(value), 255 );
}

/// Add RGB components properly.
int addColor(int c1, float multiply, int c2)
{
    return multiply > 0.0f
            ? static_cast<int>( std::sqrt(c1*c1 + multiply * c2*c2) )
            : c1 + static_cast<int>(multiply * c2);
}

void addColor(
        const QString &color, float multiply, int *r, int *g, int *b, int *a,
        const Theme &theme, int maxRecursion)
{
    if (color.isEmpty())
        return;

    QColor toAdd;
    float x = multiply;

    if (color.at(0).isDigit()) {
        bool ok;
        x = multiply * color.toFloat(&ok);
        if (!ok)
            return;
        toAdd = QColor(1, 1, 1);
    } else if ( color.startsWith('#') || color.startsWith("rgba(") ) {
        toAdd = deserializeColor(color);
    } else if (maxRecursion > 0) {
        toAdd = evalColor(theme.value(color).toString(), theme, maxRecursion - 1);
    }

    *r = normalizeColorValue( addColor(*r, x, toAdd.red()) );
    *g = normalizeColorValue( addColor(*g, x, toAdd.green()) );
    *b = normalizeColorValue( addColor(*b, x, toAdd.blue()) );
    if (multiply > 0.0f)
        *a = normalizeColorValue(*a + x * toAdd.alpha());
}

QString getFontStyleSheet(const QString &fontString, double scale = 1.0)
{
    QFont font;
    if (!fontString.isEmpty())
        font.fromString(fontString);

    qreal size = font.pointSizeF() * scale;
    QString sizeUnits = "pt";
    if (size < 0.0) {
        size = font.pixelSize() * scale;
        sizeUnits = "px";
    }

    QString result;
    result.append( QString(";font-family: \"%1\"").arg(font.family()) );
    result.append( QString(";font:%1 %2 %3%4")
                   .arg(font.style() == QFont::StyleItalic
                        ? "italic" : font.style() == QFont::StyleOblique ? "oblique" : "normal",
                        font.bold() ? "bold" : "")
                   .arg(size)
                   .arg(sizeUnits)
                   );
    result.append( QString(";text-decoration:%1 %2 %3")
                   .arg(font.strikeOut() ? "line-through" : "",
                        font.underline() ? "underline" : "",
                        font.overline() ? "overline" : "")
                   );
    // QFont::weight -> CSS
    // (normal) 50 -> 400
    // (bold)   75 -> 700
    int w = font.weight() * 12 - 200;
    result.append( QString(";font-weight:%1").arg(w) );

    result.append(";");

    return result;
}

int itemMargin()
{
    const int dpi = QApplication::desktop()->physicalDpiX();
    return std::max(4, dpi / 30);
}

} // namespace

Theme::Theme(const QSettings &settings)
{
    loadTheme(settings);
}

Theme::Theme(Ui::ConfigTabAppearance *ui)
    : ui(ui)
{
}

void Theme::loadTheme(const QSettings &settings)
{
    resetTheme();

    for ( const auto &key : m_theme.keys() ) {
        const auto value = settings.value(key);
        if ( value.isValid() )
            m_theme[key].setValue(value);
    }

    const auto margin = itemMargin();
    m_margins = QSize(margin * 2 + 6, margin);

    // search style
    m_searchPalette.setColor(QPalette::Base, color("find_bg"));
    m_searchPalette.setColor(QPalette::Text, color("find_fg"));
    m_searchFont = font("find_font");

    // editor style
    m_editorPalette.setColor(QPalette::Base, color("edit_bg"));
    m_editorPalette.setColor(QPalette::Text, color("edit_fg"));
    m_editorFont = font("edit_font");

    // number style
    m_showRowNumber = value("show_number").toBool();
    m_rowNumberPalette.setColor(QPalette::Text, color("num_fg"));
    m_rowNumberFont = font("num_font");
    m_rowNumberSize = QFontMetrics(m_rowNumberFont).boundingRect( QString("0123") ).size()
            + QSize(m_margins.width() / 2, 2 * m_margins.height());

    m_antialiasing = isAntialiasingEnabled();

}

void Theme::saveTheme(QSettings *settings) const
{
    QStringList keys = m_theme.keys();
    keys.sort();

    for (const auto &key : keys)
        settings->setValue( key, value(key) );
}

QVariant Theme::value(const QString &name) const
{
    return m_theme.value(name).value();
}

QColor Theme::color(const QString &name) const
{
    return evalColor( value(name).toString(), *this );
}

QFont Theme::font(const QString &name) const
{
    return themeFontFromString( value(name).toString() );
}

QColor Theme::evalColorExpression(const QString &expr) const
{
    return evalColor( expr, *this );
}

void Theme::decorateBrowser(QListView *c) const
{
    QAbstractScrollArea *c2 = c;
    decorateBrowser(c2);

    bool ok;
    const int itemSpacing = value("item_spacing").toInt(&ok);
    c->setSpacing( ok ? itemSpacing : c->fontMetrics().lineSpacing() / 6 );
}

void Theme::decorateMainWindow(QWidget *mainWindow) const
{
    if ( !isMainWindowThemeEnabled() ) {
        mainWindow->setStyleSheet(QString());
        return;
    }

    mainWindow->setStyleSheet(
        "MainWindow{background:" + themeColorString("bg") + "}"

        "#searchBar{"
        + themeStyleSheet("search_bar") +
        "}"

        "#searchBar:focus{"
        + themeStyleSheet("search_bar_focused") +
        "}"

        "#tab_bar{" + themeStyleSheet("tab_bar_css") + "}"
        "#tab_bar::tab:selected{" + themeStyleSheet("tab_bar_tab_selected_css") + "}"
        "#tab_bar::tab:!selected{" + themeStyleSheet("tab_bar_tab_unselected_css") + "}"
        "#tab_bar #tab_item_counter{" + themeStyleSheet("tab_bar_item_counter") + "}"
        "#tab_bar #tab_item_counter[CopyQ_selected=\"true\"]{"
        + themeStyleSheet("tab_bar_sel_item_counter") +
        "}"

        "#tab_bar QToolButton{" + themeStyleSheet("tab_bar_scroll_buttons_css") + "}"

        "#tab_tree, #tab_tree_item{" + themeStyleSheet("tab_tree_css") + "}"

        // GTK has incorrect background for branches
        "#tab_tree::branch:selected{background:" + themeColorString("bg") + "}"

        "#tab_tree::item:selected"
        ",#tab_tree_item[CopyQ_selected=\"true\"]"
        "{"
        + themeStyleSheet("tab_tree_sel_item_css") +
        "}"

        "#tab_tree_item[CopyQ_selected=\"false\"]"
        ",#tab_tree_item[CopyQ_selected=\"true\"]"
        "{background:transparent}"

        "#tab_tree #tab_item_counter{" + themeStyleSheet("tab_tree_item_counter") + "}"
        "#tab_tree #tab_item_counter[CopyQ_selected=\"true\"]{"
        + themeStyleSheet("tab_tree_sel_item_counter") +
        "}"

        // Remove border in toolbars.
        "QToolBar{border:none}"
        "QToolBar QToolButton{color:" + themeColorString("fg") + "}"

        "#menu_bar, #menu_bar::item {"
          + themeStyleSheet("menu_bar_css") + "}"
        "#menu_bar::item:selected {"
          + themeStyleSheet("menu_bar_selected_css") + "}"
        "#menu_bar::item:disabled {"
          + themeStyleSheet("menu_bar_disabled_css") + "}"

        + getMenuStyleSheet("#menu_bar")
        + getMenuStyleSheet("#centralWidget")

        + themeStyleSheet("css")
    );
}

void Theme::decorateToolBar(QWidget *toolBar) const
{
    if ( isMainWindowThemeEnabled() ) {
        toolBar->setStyleSheet(
            "QToolBar{" + themeStyleSheet("tool_bar_css") + "}"
            "QToolButton{" + themeStyleSheet("tool_button_css") + "}"
            "QToolButton:hover{" + themeStyleSheet("tool_button_selected_css") + "}"
                    );
    } else {
        toolBar->setStyleSheet(QString());
    }
}

void Theme::decorateScrollArea(QAbstractScrollArea *scrollArea) const
{
    const Qt::ScrollBarPolicy scrollbarPolicy =
            value("show_scrollbars").toBool()
            ? Qt::ScrollBarAsNeeded
            : Qt::ScrollBarAlwaysOff;
    scrollArea->setVerticalScrollBarPolicy(scrollbarPolicy);
    scrollArea->setHorizontalScrollBarPolicy(scrollbarPolicy);
}

void Theme::decorateItemPreview(QAbstractScrollArea *itemPreview) const
{
    decorateBrowser(itemPreview);
}

QString Theme::getToolTipStyleSheet() const
{
    const QString fg = themeColorString("notes_fg");
    return QString("#item QToolTip, QMenu QToolTip {")
            + getFontStyleSheet( value("notes_font").toString() )
            + ";background:" + themeColorString("notes_bg")
            + ";color:" + fg
            // Reseting border helps in some cases to set correct backgroung color.
            + ";border:1px solid " + fg
            + ";" + themeStyleSheet("notes_css")
            + "}";
}

QString Theme::getNotificationStyleSheet() const
{
    // Notification style sheet.
    QColor notificationBg = color("notification_bg");
    // Notification opacity should be set with NotificationDaemon::setNotificationOpacity().
    notificationBg.setAlpha(255);

    const QString fontString = value("notification_font").toString();

    return "Notification, Notification QWidget{"
           "background:" + serializeColor(notificationBg) + ";"
           "}"
           "Notification QWidget{"
           "color:" + themeColorString("notification_fg") + ";"
           + getFontStyleSheet(fontString) +
           "}"
           "Notification #NotificationTitle{"
           + getFontStyleSheet(fontString, 1.2) +
           "}"
           "Notification #NotificationTip{"
           "font-style: italic"
           "}"
            ;
}

QFont Theme::themeFontFromString(const QString &fontString) const
{
    QFont font;
    if ( !fontString.isEmpty() )
        font.fromString(fontString);
    if ( !isAntialiasingEnabled() )
        font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

bool Theme::isAntialiasingEnabled() const
{
    return m_antialiasing;
}

void Theme::resetTheme()
{
    QString name;
    QPalette p;
    name = serializeColor( p.color(QPalette::Base) );
    m_theme["bg"]          = Option(name, "VALUE", ui ? ui->pushButtonColorBg : nullptr);
    m_theme["edit_bg"]     = Option(name, "VALUE", ui ? ui->pushButtonColorEditorBg : nullptr);
    name = serializeColor( p.color(QPalette::Text) );
    m_theme["fg"]          = Option(name, "VALUE", ui ? ui->pushButtonColorFg : nullptr);
    m_theme["edit_fg"]     = Option(name, "VALUE", ui ? ui->pushButtonColorEditorFg : nullptr);
    name = serializeColor( p.color(QPalette::Text).lighter(400) );
    m_theme["num_fg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorNumberFg : nullptr);
    name = serializeColor( p.color(QPalette::AlternateBase) );
    m_theme["alt_bg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorAltBg : nullptr);
    name = serializeColor( p.color(QPalette::Highlight) );
    m_theme["sel_bg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorSelBg : nullptr);
    name = serializeColor( p.color(QPalette::HighlightedText) );
    m_theme["sel_fg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorSelFg : nullptr);
    m_theme["find_bg"]     = Option("#ff0", "VALUE", ui ? ui->pushButtonColorFoundBg : nullptr);
    m_theme["find_fg"]     = Option("#000", "VALUE", ui ? ui->pushButtonColorFoundFg : nullptr);
    name = serializeColor( p.color(QPalette::ToolTipBase) );
    m_theme["notes_bg"]  = Option(name, "VALUE", ui ? ui->pushButtonColorNotesBg : nullptr);
    name = serializeColor( p.color(QPalette::ToolTipText) );
    m_theme["notes_fg"]  = Option(name, "VALUE", ui ? ui->pushButtonColorNotesFg : nullptr);
    m_theme["notification_bg"]  = Option("#333", "VALUE", ui ? ui->pushButtonColorNotificationBg : nullptr);
    m_theme["notification_fg"]  = Option("#ddd", "VALUE", ui ? ui->pushButtonColorNotificationFg : nullptr);

    m_theme["font"]        = Option("", "VALUE", ui ? ui->pushButtonFont : nullptr);
    m_theme["edit_font"]   = Option("", "VALUE", ui ? ui->pushButtonEditorFont : nullptr);
    m_theme["find_font"]   = Option("", "VALUE", ui ? ui->pushButtonFoundFont : nullptr);
    m_theme["num_font"]    = Option("", "VALUE", ui ? ui->pushButtonNumberFont : nullptr);
    m_theme["notes_font"]  = Option("", "VALUE", ui ? ui->pushButtonNotesFont : nullptr);
    m_theme["notification_font"]  = Option("", "VALUE", ui ? ui->pushButtonNotificationFont : nullptr);
    m_theme["show_number"] = Option(true, "checked", ui ? ui->checkBoxShowNumber : nullptr);
    m_theme["show_scrollbars"] = Option(true, "checked", ui ? ui->checkBoxScrollbars : nullptr);

    m_theme["css"] = Option("");
    m_theme["menu_css"] = Option(
                "\n    ;border-top: 0.08em solid ${bg + #333}"
                "\n    ;border-left: 0.08em solid ${bg + #333}"
                "\n    ;border-bottom: 0.08em solid ${bg - #333}"
                "\n    ;border-right: 0.08em solid ${bg - #333}"
                );
    m_theme["menu_bar_css"] = Option(
                "\n    ;background: ${bg}"
                "\n    ;color: ${fg}"
                );
    m_theme["menu_bar_selected_css"] = Option(
                "\n    ;background: ${sel_bg}"
                "\n    ;color: ${sel_fg}"
                );
    m_theme["menu_bar_disabled_css"] = Option(
                "\n    ;color: ${bg - #666}"
                );

    m_theme["item_css"] = Option("");
    m_theme["alt_item_css"] = Option("");
    m_theme["sel_item_css"] = Option("");
    m_theme["cur_item_css"] = Option(
                "\n    ;border: 0.1em solid ${sel_bg}"
                );
    m_theme["item_spacing"] = Option("");
    m_theme["notes_css"] = Option("");

    m_theme["tab_bar_css"] = Option(
                "\n    ;background: ${bg - #222}"
                );
    m_theme["tab_bar_tab_selected_css"] = Option(
                "\n    ;padding: 0.5em"
                "\n    ;background: ${bg}"
                "\n    ;border: 0.05em solid ${bg}"
                "\n    ;color: ${fg}"
                );
    m_theme["tab_bar_tab_unselected_css"] = Option(
                "\n    ;border: 0.05em solid ${bg}"
                "\n    ;padding: 0.5em"
                "\n    ;background: ${bg - #222}"
                "\n    ;color: ${fg - #333}"
                );
    m_theme["tab_bar_scroll_buttons_css"] = Option(
                "\n    ;background: ${bg - #222}"
                "\n    ;color: ${fg}"
                "\n    ;border: 0"
                );
    m_theme["tab_bar_item_counter"] = Option(
                "\n    ;color: ${fg - #044 + #400}"
                "\n    ;font-size: 6pt"
                );
    m_theme["tab_bar_sel_item_counter"] = Option(
                "\n    ;color: ${sel_bg - #044 + #400}"
                );

    m_theme["tab_tree_css"] = Option(
                "\n    ;color: ${fg}"
                "\n    ;background-color: ${bg}"
                );
    m_theme["tab_tree_sel_item_css"] = Option(
                "\n    ;color: ${sel_fg}"
                "\n    ;background-color: ${sel_bg}"
                "\n    ;border-radius: 2px"
                );
    m_theme["tab_tree_item_counter"] = Option(
                "\n    ;color: ${fg - #044 + #400}"
                "\n    ;font-size: 6pt"
                );
    m_theme["tab_tree_sel_item_counter"] = Option(
                "\n    ;color: ${sel_fg - #044 + #400}"
                );

    m_theme["tool_bar_css"] = Option(
                "\n    ;color: ${fg}"
                "\n    ;background-color: ${bg}"
                "\n    ;border: 0"
                );
    m_theme["tool_button_css"] = Option(
                "\n    ;background-color: transparent"
                );
    m_theme["tool_button_selected_css"] = Option(
                "\n    ;background: ${sel_bg}"
                "\n    ;color: ${sel_fg}"
                );

    m_theme["search_bar"] = Option(
                "\n    ;background: ${edit_bg}"
                "\n    ;color: ${edit_fg}"
                "\n    ;border: 1px solid ${alt_bg}"
                "\n    ;margin: 2px"
                );
    m_theme["search_bar_focused"] = Option(
                "\n    ;border: 1px solid ${sel_bg}"
                );

    m_theme["use_system_icons"] = Option(false, "checked", ui ? ui->checkBoxSystemIcons : nullptr);
    m_theme["font_antialiasing"] = Option(true, "checked", ui ? ui->checkBoxAntialias : nullptr);
    m_theme["style_main_window"] = Option(false, "checked", ui ? ui->checkBoxStyleMainWindow : nullptr);
}

void Theme::decorateBrowser(QAbstractScrollArea *c) const
{
    decorateScrollArea(c);

    const QColor bg = color("bg");
    QColor unfocusedSelectedBg = color("sel_bg");
    unfocusedSelectedBg.setRgb(
                (bg.red() + unfocusedSelectedBg.red()) / 2,
                (bg.green() + unfocusedSelectedBg.green()) / 2,
                (bg.blue() + unfocusedSelectedBg.blue()) / 2
                );

    Theme unfocusedTheme;
    for (auto it = m_theme.constBegin(); it != m_theme.constEnd(); ++it)
        unfocusedTheme.m_theme[it.key()] = Option(it.value().value());
    unfocusedTheme.m_theme["sel_bg"].setValue( serializeColor(unfocusedSelectedBg) );

    // colors and font
    c->setStyleSheet(
        "#ClipboardBrowser,#item,#item_child{"
          + getFontStyleSheet( value("font").toString() ) +
          "color:" + themeColorString("fg") + ";"
          "background:" + themeColorString("bg") + ";"
        "}"

        "#ClipboardBrowser::item:alternate{"
          "color:" + themeColorString("alt_fg") + ";"
          "background:" + themeColorString("alt_bg") + ";"
        "}"

        "#ClipboardBrowser::item:selected,#item[CopyQ_selected=\"true\"],#item[CopyQ_selected=\"true\"] #item_child{"
          "color:" + themeColorString("sel_fg") + ";"
          "background:" + themeColorString("sel_bg") + ";"
        "}"

        "#item #item_child{background:transparent}"
        "#item[CopyQ_selected=\"true\"] #item_child{background:transparent}"

        // Desaturate selected item background if item list is not focused.
        "#ClipboardBrowser::item:selected:!active{"
          "background:" + serializeColor( evalColor("sel_bg", unfocusedTheme) ) + ";"
          + unfocusedTheme.themeStyleSheet("sel_item_css") +
        "}"

        "#ClipboardBrowser::item:focus{"
          + themeStyleSheet("cur_item_css") +
        "}"

        + getToolTipStyleSheet() +

        // Allow user to change CSS.
        "#ClipboardBrowser{" + themeStyleSheet("item_css") + "}"
        "#ClipboardBrowser::item:alternate{" + themeStyleSheet("alt_item_css") + "}"
        "#ClipboardBrowser::item:selected{" + themeStyleSheet("sel_item_css") + "}"

        "#item_child[CopyQ_item_type=\"notes\"] {"
          + getFontStyleSheet( value("notes_font").toString() ) +
        "}"
    );
}

bool Theme::isMainWindowThemeEnabled() const
{
    return m_theme.value("style_main_window").value().toBool();
}

QString Theme::themeStyleSheet(const QString &name) const
{
    QString css = value(name).toString();
    int i = 0;

    forever {
        i = css.indexOf("${", i);
        if (i == -1)
            break;
        int j = css.indexOf('}', i + 2);
        if (j == -1)
            break;

        const QString var = css.mid(i + 2, j - i - 2);

        const QString colorName = serializeColor( evalColor(var, *this) );
        css.replace(i, j - i + 1, colorName);
        i += colorName.size();
    }

    return css;
}

QString Theme::themeColorString(const QString &name) const
{
    return serializeColor( color(name) );
}

QString Theme::getMenuStyleSheet(const QString &selector) const
{
    const int iconSize = iconFontSizePixels();
    return
        // Remove icon border in menus.
        selector + " QMenu::item:selected{border:none}"
        + selector + " QMenu::item{"
          ";padding:0.2em 1em 0.2em 1em"
          ";padding-left:" + QString::number(iconSize * 2) + "px}"
        + selector + " QMenu::icon{padding-left:" + QString::number(iconSize / 2) + "px}"

        // Keep default item highlighted (removing icon border resets the style).
        + selector + " QMenu::item:default{font-weight:bold}"

        + selector + " QMenu {" + themeStyleSheet("menu_css") + "}"

        + selector + " QMenu,"
        + selector + " QMenu::item,"
        + selector + " QMenu::separator {"
          + themeStyleSheet("menu_bar_css") + "}"
        + selector + " QMenu::item:selected {"
          + themeStyleSheet("menu_bar_selected_css") + "}"
        + selector + "  QMenu::item:disabled {"
          + themeStyleSheet("menu_bar_disabled_css") + "}";
}

QString serializeColor(const QColor &color)
{
    if (color.alpha() == 255)
        return color.name();

    return QString("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(color.alpha());
}

QColor deserializeColor(const QString &colorName)
{
    if ( colorName.startsWith("rgba(") ) {
        QStringList list = colorName.mid(5, colorName.indexOf(')') - 5).split(',');
        int r = list.value(0).toInt();
        int g = list.value(1).toInt();
        int b = list.value(2).toInt();
        int a = static_cast<int>( list.value(3).toDouble() * 255 );

        return QColor(r, g, b, a > 255 ? a / 255 : a);
    }

    return QColor(colorName);
}

QColor evalColor(const QString &expression, const Theme &theme, int maxRecursion)
{
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;

    QStringList addList = QString(expression).remove(' ').split('+');
    for (const auto &add : addList) {
        QStringList subList = add.split('-');
        float multiply = 1;
        for (const auto &sub : subList) {
            addColor(sub, multiply, &r, &g, &b, &a, theme, maxRecursion);
            multiply = -1;
        }
    }

    return QColor(r, g, b, a);
}

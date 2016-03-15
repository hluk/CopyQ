/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "item/itemdelegate.h"

#include "gui/iconfont.h"

#include <QListView>
#include <QSettings>

namespace {

int normalizeColorValue(int value)
{
    return qBound(0, value, 255);
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
        toAdd = QColor(Qt::black);
    } else if ( color.startsWith('#') || color.startsWith("rgba(") ) {
        toAdd = deserializeColor(color);
    } else if (maxRecursion > 0) {
        toAdd = evalColor(theme.value(color).toString(), theme, maxRecursion - 1);
    }

    *r = normalizeColorValue(*r + x * toAdd.red());
    *g = normalizeColorValue(*g + x * toAdd.green());
    *b = normalizeColorValue(*b + x * toAdd.blue());
    if (multiply > 0.0)
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
                        ? "italic" : font.style() == QFont::StyleOblique ? "oblique" : "normal")
                   .arg(font.bold() ? "bold" : "")
                   .arg(size)
                   .arg(sizeUnits)
                   );
    result.append( QString(";text-decoration:%1 %2 %3")
                   .arg(font.strikeOut() ? "line-through" : "")
                   .arg(font.underline() ? "underline" : "")
                   .arg(font.overline() ? "overline" : "")
                   );
    // QFont::weight -> CSS
    // (normal) 50 -> 400
    // (bold)   75 -> 700
    int w = font.weight() * 12 - 200;
    result.append( QString(";font-weight:%1").arg(w) );

    result.append(";");

    return result;
}

} // namespace

Theme::Theme()
    : ui(NULL)
{
    QSettings settings;
    settings.beginGroup("Theme");
    loadTheme(settings);
    settings.endGroup();
}

Theme::Theme(QSettings &settings)
    : ui(NULL)
{
    loadTheme(settings);
}

Theme::Theme(Ui::ConfigTabAppearance *ui)
    : ui(ui)
{
}

void Theme::loadTheme(QSettings &settings)
{
    resetTheme();

    foreach ( const QString &key, m_theme.keys() ) {
        if ( settings.contains(key) ) {
            const QVariant value = settings.value(key);
            if ( value.isValid() )
                m_theme[key].setValue(value);
        }
    }
}

void Theme::saveTheme(QSettings &settings) const
{
    QStringList keys = m_theme.keys();
    keys.sort();

    foreach (const QString &key, keys)
        settings.setValue( key, value(key) );
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

void Theme::decorateBrowser(QListView *c, ItemDelegate *d) const
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
    foreach ( const QString &key, m_theme.keys() )
        unfocusedTheme.m_theme[key] = Option(m_theme[key].value());
    unfocusedTheme.m_theme["sel_bg"].setValue( serializeColor(unfocusedSelectedBg) );

    // colors and font
    c->setStyleSheet(
        "ClipboardBrowser,#item,#item_child{"
          + getFontStyleSheet( value("font").toString() ) +
          "color:" + themeColorString("fg") + ";"
          "background:" + themeColorString("bg") + ";"
        "}"

        "ClipboardBrowser::item:alternate{"
          "color:" + themeColorString("alt_fg") + ";"
          "background:" + themeColorString("alt_bg") + ";"
        "}"

        "ClipboardBrowser::item:selected,#item[CopyQ_selected=\"true\"],#item[CopyQ_selected=\"true\"] #item_child{"
          "color:" + themeColorString("sel_fg") + ";"
          "background:" + themeColorString("sel_bg") + ";"
        "}"

        "#item,#item #item_child{background:transparent}"
        "#item[CopyQ_selected=\"true\"],#item[CopyQ_selected=\"true\"] #item_child{background:transparent}"

        // Desaturate selected item background if item list is not focused.
        "ClipboardBrowser::item:selected:!active{"
          "background:" + serializeColor( evalColor("sel_bg", unfocusedTheme) ) + ";"
          + unfocusedTheme.themeStyleSheet("sel_item_css") +
        "}"

        "ClipboardBrowser::item:focus{"
          + themeStyleSheet("cur_item_css") +
        "}"

        + getToolTipStyleSheet() +

        // Allow user to change CSS.
        "ClipboardBrowser{" + themeStyleSheet("item_css") + "}"
        "ClipboardBrowser::item:alternate{" + themeStyleSheet("alt_item_css") + "}"
        "ClipboardBrowser::item:selected{" + themeStyleSheet("sel_item_css") + "}"

        "#item_child[CopyQ_item_type=\"notes\"] {"
          + getFontStyleSheet( value("notes_font").toString() ) +
        "}"
    );

    QPalette p;

    // search style
    p.setColor(QPalette::Base, color("find_bg"));
    p.setColor(QPalette::Text, color("find_fg"));
    d->setSearchStyle(font("find_font"), p);

    // editor style
    p.setColor(QPalette::Base, color("edit_bg"));
    p.setColor(QPalette::Text, color("edit_fg"));
    d->setEditorStyle(font("edit_font"), p);

    // number style
    d->setRowNumberVisibility(value("show_number").toBool());
    p.setColor(QPalette::Text, color("num_fg"));
    d->setNumberStyle(font("num_font"), p);

    d->setFontAntialiasing( isAntialiasingEnabled() );

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

    const int iconSize = iconFontSizePixels();
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

        // Remove icon border in menus.
        "#menu_bar QMenu::item:selected{border:none}"
        "#menu_bar QMenu::item{"
          ";padding:0.2em 1em 0.2em 1em"
          ";padding-left:" + QString::number(iconSize * 2) + "px}"
        "#menu_bar QMenu::icon{padding-left:" + QString::number(iconSize / 2) + "px}"

        // Keep default item highlighted (removing icon border resets the style).
        "#menu_bar QMenu::item:default{font-weight:bold}"

        "#menu_bar QMenu {" + themeStyleSheet("menu_css") + "}"
        "#menu_bar, #menu_bar::item, #menu_bar QMenu, #menu_bar QMenu::item, #menu_bar QMenu::separator {"
          + themeStyleSheet("menu_bar_css") + "}"
        "#menu_bar::item:selected, #menu_bar QMenu::item:selected {"
          + themeStyleSheet("menu_bar_selected_css") + "}"
        "#menu_bar::item:disabled, #menu_bar QMenu::item:disabled {"
          + themeStyleSheet("menu_bar_disabled_css") + "}"

        + themeStyleSheet("css")
    );
}

void Theme::decorateToolBar(QWidget *toolBar) const
{
    if ( isMainWindowThemeEnabled() ) {
        toolBar->setStyleSheet(
            "QToolBar{" + themeStyleSheet("tool_bar_css") + "}"
            "QToolButton{" + themeStyleSheet("tool_button_css") + "}"
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

    return "Notification{"
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
    font.fromString(fontString);
    if ( !isAntialiasingEnabled() )
        font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

bool Theme::isAntialiasingEnabled() const
{
    return value("font_antialiasing").toBool();
}

void Theme::resetTheme()
{
    QString name;
    QPalette p;
    name = serializeColor( p.color(QPalette::Base) );
    m_theme["bg"]          = Option(name, "VALUE", ui ? ui->pushButtonColorBg : NULL);
    m_theme["edit_bg"]     = Option(name, "VALUE", ui ? ui->pushButtonColorEditorBg : NULL);
    name = serializeColor( p.color(QPalette::Text) );
    m_theme["fg"]          = Option(name, "VALUE", ui ? ui->pushButtonColorFg : NULL);
    m_theme["edit_fg"]     = Option(name, "VALUE", ui ? ui->pushButtonColorEditorFg : NULL);
    name = serializeColor( p.color(QPalette::Text).lighter(400) );
    m_theme["num_fg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorNumberFg : NULL);
    name = serializeColor( p.color(QPalette::AlternateBase) );
    m_theme["alt_bg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorAltBg : NULL);
    name = serializeColor( p.color(QPalette::Highlight) );
    m_theme["sel_bg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorSelBg : NULL);
    name = serializeColor( p.color(QPalette::HighlightedText) );
    m_theme["sel_fg"]      = Option(name, "VALUE", ui ? ui->pushButtonColorSelFg : NULL);
    m_theme["find_bg"]     = Option("#ff0", "VALUE", ui ? ui->pushButtonColorFoundBg : NULL);
    m_theme["find_fg"]     = Option("#000", "VALUE", ui ? ui->pushButtonColorFoundFg : NULL);
    name = serializeColor( p.color(QPalette::ToolTipBase) );
    m_theme["notes_bg"]  = Option(name, "VALUE", ui ? ui->pushButtonColorNotesBg : NULL);
    name = serializeColor( p.color(QPalette::ToolTipText) );
    m_theme["notes_fg"]  = Option(name, "VALUE", ui ? ui->pushButtonColorNotesFg : NULL);
    m_theme["notification_bg"]  = Option("#333", "VALUE", ui ? ui->pushButtonColorNotificationBg : NULL);
    m_theme["notification_fg"]  = Option("#ddd", "VALUE", ui ? ui->pushButtonColorNotificationFg : NULL);

    m_theme["font"]        = Option("", "VALUE", ui ? ui->pushButtonFont : NULL);
    m_theme["edit_font"]   = Option("", "VALUE", ui ? ui->pushButtonEditorFont : NULL);
    m_theme["find_font"]   = Option("", "VALUE", ui ? ui->pushButtonFoundFont : NULL);
    m_theme["num_font"]    = Option("", "VALUE", ui ? ui->pushButtonNumberFont : NULL);
    m_theme["notes_font"]  = Option("", "VALUE", ui ? ui->pushButtonNotesFont : NULL);
    m_theme["notification_font"]  = Option("", "VALUE", ui ? ui->pushButtonNotificationFont : NULL);
    m_theme["show_number"] = Option(true, "checked", ui ? ui->checkBoxShowNumber : NULL);
    m_theme["show_scrollbars"] = Option(true, "checked", ui ? ui->checkBoxScrollbars : NULL);

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

    m_theme["search_bar"] = Option(
                "\n    ;background: ${edit_bg}"
                "\n    ;color: ${edit_fg}"
                "\n    ;border: 1px solid ${alt_bg}"
                "\n    ;margin: 2px"
                );
    m_theme["search_bar_focused"] = Option(
                "\n    ;border: 1px solid ${sel_bg}"
                );

    m_theme["use_system_icons"] = Option(false, "checked", ui ? ui->checkBoxSystemIcons : NULL);
    m_theme["font_antialiasing"] = Option(true, "checked", ui ? ui->checkBoxAntialias : NULL);
    m_theme["style_main_window"] = Option(false, "checked", ui ? ui->checkBoxStyleMainWindow : NULL);
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
        int a = list.value(3).toDouble() * 255;

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
    foreach (const QString &add, addList) {
        QStringList subList = add.split('-');
        float multiply = 1;
        foreach (const QString &sub, subList) {
            addColor(sub, multiply, &r, &g, &b, &a, theme, maxRecursion);
            multiply = -1;
        }
    }

    return QColor(r, g, b, a);
}

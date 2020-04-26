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

#include "theme.h"

#include "ui_configtabappearance.h"

#include "common/log.h"
#include "gui/iconfont.h"
#include "platform/platformnativeinterface.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QListView>
#include <QSettings>
#include <QStyleFactory>

#include <cmath>

namespace {

double normalizeFactor(double value)
{
    return qBound( 0.0, value, 1.0 );
}

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
        const Theme &theme, const Values &values, int maxRecursion)
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
        if ( !toAdd.isValid() )
            toAdd = QColor(Qt::black);
    } else if (maxRecursion > 0) {
        const QVariant v = values.contains(color)
                ? values.value(color) : theme.value(color);
        toAdd = evalColor(v.toString(), theme, values, maxRecursion - 1);
    }

    *r = normalizeColorValue( addColor(*r, x, toAdd.red()) );
    *g = normalizeColorValue( addColor(*g, x, toAdd.green()) );
    *b = normalizeColorValue( addColor(*b, x, toAdd.blue()) );
    if (multiply > 0.0f)
        *a = normalizeColorValue(*a + x * toAdd.alpha());
}

int fitFontWeight(int weight, int low, int high, int highCss)
{
    const int diffCss = 100;
    const int lowCss = highCss - diffCss;
    if (weight == low)
        return lowCss;

    const int diff = high - low;
    return lowCss + diffCss * weight / diff;
}

int getFontWeightForStyleSheet(int weight)
{
    // Copied from QFont sources (not available in all Qt versions).
    enum Weight {
        Thin     = 0,    // 100
        ExtraLight = 12, // 200
        Light    = 25,   // 300
        Normal   = 50,   // 400
        Medium   = 57,   // 500
        DemiBold = 63,   // 600
        Bold     = 75,   // 700
        ExtraBold = 81,  // 800
        Black    = 87    // 900
    };

    if (weight < ExtraLight)
        return fitFontWeight(weight, Thin, ExtraLight, 200);
    if (weight < Light)
        return fitFontWeight(weight, ExtraLight, Light, 300);
    if (weight < Normal)
        return fitFontWeight(weight, Light, Normal, 400);
    if (weight < Medium)
        return fitFontWeight(weight, Normal, Medium, 500);
    if (weight < DemiBold)
        return fitFontWeight(weight, Medium, DemiBold, 600);
    if (weight < Bold)
        return fitFontWeight(weight, DemiBold, Bold, 700);
    if (weight < ExtraBold)
        return fitFontWeight(weight, Bold, ExtraBold, 800);
    if (weight < Black)
        return fitFontWeight(weight, ExtraBold, Black, 900);

    return 900;
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
    size = qBound(0.1, size, 50.0);

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

    const int w = getFontWeightForStyleSheet( font.weight() );
    result.append( QString(";font-weight:%1").arg(w) );

    result.append(";");

    return result;
}

int itemMargin()
{
    const int dpi = QApplication::desktop()->physicalDpiX();
    return std::max(2, dpi / 72);
}

QString themePrefix()
{
#ifdef COPYQ_THEME_PREFIX
    return COPYQ_THEME_PREFIX;
#else
    return platformNativeInterface()->themePrefix();
#endif
}

double getFactor(const QString &name, const QHash<QString, QVariant> &values)
{
    bool ok;
    const double scale = values.value(name).toDouble(&ok);
    return ok ? scale : 1.0;
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

    updateTheme();
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
    QPalette palette = QApplication::palette();

    // This seems to properly reset icon colors.
    mainWindow->setStyleSheet(QString());
    mainWindow->setPalette(palette);

    if ( !isMainWindowThemeEnabled() )
        return;

    const auto bg = color("bg");
    const auto fg = color("fg");
    palette.setColor( QPalette::Base, bg );
    palette.setColor( QPalette::AlternateBase, color("alt_bg") );
    palette.setColor( QPalette::Text, fg );
    palette.setColor( QPalette::Window, bg );
    palette.setColor( QPalette::WindowText, fg );
    palette.setColor( QPalette::Button, bg );
    palette.setColor( QPalette::ButtonText, fg );
    palette.setColor( QPalette::Highlight, color("sel_bg") );
    palette.setColor( QPalette::HighlightedText, color("sel_fg") );

    mainWindow->setPalette(palette);
    const QString cssTemplate = value("css_template_main_window").toString();
    mainWindow->setStyleSheet(getStyleSheet(cssTemplate));
}

void Theme::decorateScrollArea(QAbstractScrollArea *scrollArea) const
{
    const auto scrollbarPolicy = this->scrollbarPolicy();
    scrollArea->setVerticalScrollBarPolicy(scrollbarPolicy);
    scrollArea->setHorizontalScrollBarPolicy(scrollbarPolicy);
}

void Theme::decorateItemPreview(QAbstractScrollArea *itemPreview) const
{
    decorateBrowser(itemPreview);
}

QString Theme::getToolTipStyleSheet() const
{
    const QString cssTemplate = value("css_template_tooltip").toString();
    return getStyleSheet(cssTemplate);
}

QString Theme::getNotificationStyleSheet() const
{
    const QString cssTemplate = value("css_template_notification").toString();
    return getStyleSheet(cssTemplate);
}

Qt::ScrollBarPolicy Theme::scrollbarPolicy() const
{
    return value("show_scrollbars").toBool()
            ? Qt::ScrollBarAsNeeded
            : Qt::ScrollBarAlwaysOff;
}

bool Theme::useSystemIcons() const
{
    return value("use_system_icons").toBool();
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
                "\n    ;border: 1px solid ${sel_bg}"
                "\n    ;background: ${bg}"
                "\n    ;color: ${fg}"
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
    m_theme["hover_item_css"] = Option("");
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
                "\n    ;color: ${fg}"
                "\n    ;background: ${bg}"
                "\n    ;border: 0"
                "\n    ;border-radius: 2px"
                );
    m_theme["tool_button_selected_css"] = Option(
                "\n    ;background: ${sel_bg - #222}"
                "\n    ;color: ${sel_fg}"
                "\n    ;border: 1px solid ${sel_bg}"
                );
    m_theme["tool_button_pressed_css"] = Option(
                "\n    ;background: ${sel_bg}"
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

    const int iconSize = iconFontSizePixels();
    m_theme["icon_size"] = Option( QString::number(iconSize) );

    m_theme["css_template_items"] = Option("items");
    m_theme["css_template_main_window"] = Option("main_window");
    m_theme["css_template_notification"] = Option("notification");
    m_theme["css_template_tooltip"] = Option("tooltip");
}

void Theme::updateTheme()
{
    const auto margin = itemMargin();
    m_margins = QSize(margin + 2, margin);

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
            + QSize(m_margins.width(), m_margins.height());

    m_antialiasing = value("font_antialiasing").toBool();
}

void Theme::decorateBrowser(QAbstractScrollArea *c) const
{
    decorateScrollArea(c);
    const QString cssTemplate = value("css_template_items").toString();
    c->setStyleSheet(getStyleSheet(cssTemplate));
}

bool Theme::isMainWindowThemeEnabled() const
{
    return m_theme.value("style_main_window").value().toBool();
}

QString Theme::themeColorString(const QString &name) const
{
    return serializeColor( color(name) );
}

QString Theme::getStyleSheet(const QString &name, Values values, int maxRecursion) const
{
    const QString fileName = findThemeFile(name + ".css");

    if ( fileName.isEmpty() )
        return QString();

    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) ) {
        log( QString("Failed to open stylesheet \"%1\": %2")
             .arg(fileName)
             .arg(file.errorString()), LogError );
        return QString();
    }

    const QString css = QString::fromUtf8( file.readAll() );
    return parseStyleSheet(css, values, maxRecursion - 1);
}

QString Theme::parseStyleSheet(const QString &css, Values values, int maxRecursion) const
{
    QString output;
    const QString variableBegin("${");
    const QString variableEnd("}");
    for ( int i = 0; i < css.size(); ++i ) {
        const int a = css.indexOf(variableBegin, i);
        if (a == -1) {
            output.append(css.mid(i));
            break;
        }

        const int b = css.indexOf(variableEnd, a + variableBegin.size());
        if (b == -1) {
            output.append(css.mid(i));
            break;
        }

        output.append(css.mid(i, a - i));
        i = b + variableEnd.size() - 1;

        const QString name = css
                .mid(a + variableBegin.size(), b - a - variableBegin.size())
                .trimmed();
        const QString result = parsePlaceholder(name, &values, maxRecursion);
        output.append(result);
    }

    return output;
}

QString Theme::parsePlaceholder(const QString &name, Values *values, int maxRecursion) const
{
    if (maxRecursion < 0) {
        log("Max recursion reached when parsing style sheet", LogError);
        return QString();
    }

    if ( name.contains("=") ) {
        const int i = name.indexOf('=');
        const QString k = name.mid(0, i).trimmed();
        const QString v = name.mid(i + 1).trimmed();
        if (k == "scale")
            (*values)[k] = 1.0;
        (*values)[k] = parsePlaceholder(v, values, maxRecursion - 1);
        return QString();
    }

    if ( name.startsWith("css:") )
        return getStyleSheet(name.mid(4), *values, maxRecursion - 1);

    const double scale = getFactor("scale", *values);

    const QString value = this->value(name).toString();
    if (name.contains("font"))
        return getFontStyleSheet(value, scale);

    if (name.contains("css") || value.contains(":"))
        return parseStyleSheet(value, *values, maxRecursion - 1);

    {
        bool ok;
        const double v = name.toDouble(&ok);
        if (ok)
            return QString::number(v * scale);
    }

    {
        bool ok;
        const double v = value.toDouble(&ok);
        if (ok)
            return QString::number(v * scale);
    }

    QColor color = evalColor(name, *this, *values);
    const double saturationF = getFactor("hsv_saturation_factor", *values);
    const double valueF = getFactor("hsv_value_factor", *values);
    if ( !qFuzzyCompare(saturationF, 1.0) || !qFuzzyCompare(valueF, 1.0) ) {
        color.setHsvF(
            color.hueF(),
            normalizeFactor(color.saturationF() * saturationF),
            normalizeFactor(color.valueF() * valueF),
            color.alphaF()
        );
    }
    return serializeColor(color);
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
    QColor color;

    if ( colorName.startsWith("rgba(") ) {
        QStringList list = colorName.mid(5, colorName.indexOf(')') - 5).split(',');
        int r = list.value(0).toInt();
        int g = list.value(1).toInt();
        int b = list.value(2).toInt();
        int a = static_cast<int>( list.value(3).toDouble() * 255 );

        color = QColor(r, g, b, a > 255 ? a / 255 : a);
    } else {
        color = QColor(colorName);
    }

    if ( !color.isValid() )
        log( QString("Failed to deserialize color \"%1\"").arg(colorName) );

    return color;
}

QColor evalColor(const QString &expression, const Theme &theme, const Values &values, int maxRecursion)
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
            addColor(sub, multiply, &r, &g, &b, &a, theme, values, maxRecursion);
            multiply = -1;
        }
    }

    return QColor(r, g, b, a);
}

QString defaultUserThemePath()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    return QDir::cleanPath(settings.fileName() + "/../themes");
}

QStringList themePaths()
{
    QStringList paths;

    const QByteArray customThemsPath = qgetenv("COPYQ_THEME_PREFIX");
    if ( !customThemsPath.isEmpty() )
        paths.append(QString::fromLocal8Bit(customThemsPath));

    const QString userThemesPath = defaultUserThemePath();
    QDir themesDir(userThemesPath);
    if ( themesDir.mkpath(".") )
        paths.append(userThemesPath);

    const QString themesPath = themePrefix();
    if ( !themesPath.isEmpty() )
        paths.append(themesPath);

    return paths;
}

QString findThemeFile(const QString &fileName)
{
    for (const QString &path : themePaths()) {
        const QDir dir(path);
        if ( dir.exists(fileName) )
            return dir.absoluteFilePath(fileName);
    }
    return QString();
}

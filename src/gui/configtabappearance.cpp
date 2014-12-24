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

#include "gui/configtabappearance.h"
#include "ui_configtabappearance.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfont.h"
#include "item/itemeditor.h"
#include "item/itemdelegate.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryFile>

#ifndef COPYQ_THEME_PREFIX
#   ifdef Q_OS_WIN
#       define COPYQ_THEME_PREFIX QApplication::applicationDirPath() + "/themes"
#   else
#       define COPYQ_THEME_PREFIX ""
#   endif
#endif

namespace {

QString getFontStyleSheet(const QString &fontString)
{
    QString result;
    if (fontString.isEmpty())
        return QString();

    QFont font;
    font.fromString(fontString);

    qreal size = font.pointSizeF();
    QString sizeUnits = "pt";
    if (size < 0.0) {
        size = font.pixelSize();
        sizeUnits = "px";
    }

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

int normalizeColorValue(int value)
{
    return qBound(0, value, 255);
}

QColor evalColor(const QString &expression, const QHash<QString, Option> &theme, int maxRecursion = 8);

void addColor(const QString &color, float multiply, int *r, int *g, int *b, int *a,
              const QHash<QString, Option> &theme, int maxRecursion)
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
    } else {
        if (maxRecursion > 0)
            toAdd = evalColor(theme.value(color).value().toString(), theme, maxRecursion - 1);
    }

    *r = normalizeColorValue(*r + x * toAdd.red());
    *g = normalizeColorValue(*g + x * toAdd.green());
    *b = normalizeColorValue(*b + x * toAdd.blue());
    if (multiply > 0.0)
        *a = normalizeColorValue(*a + x * toAdd.alpha());
}

QColor evalColor(const QString &expression, const QHash<QString, Option> &theme, int maxRecursion)
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

} // namespace

ConfigTabAppearance::ConfigTabAppearance(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigTabAppearance)
    , m_theme()
    , m_editor()
{
    ui->setupUi(this);

    const QString searchFor = tr("item", "Search expression in preview in Appearance tab.");

    ClipboardBrowser *c = ui->clipboardBrowserPreview;
    c->addItems( QStringList()
                 << tr("Search string is %1.").arg( quoteString(searchFor) )
                 << tr("Select an item and\n"
                       "press F2 to edit.")
                 << tr("Select items and move them with\n"
                       "CTRL and up or down key.")
                 << tr("Remove item with Delete key.") );
    for (int i = 1; i <= 20; ++i)
        c->add( tr("Example item %1").arg(i), -1 );

    QAbstractItemModel *model = c->model();
    QModelIndex index = model->index(0, 0);
    QVariantMap dataMap;
    dataMap.insert( mimeItemNotes, tr("Some random notes (Shift+F2 to edit)").toUtf8() );
    model->setData(index, dataMap, contentType::updateData);

    // Highlight found text but don't filter out any items.
    c->filterItems( QRegExp(QString("|") + searchFor, Qt::CaseInsensitive) );

    QAction *act;

    act = new QAction(c);
    act->setShortcut( QString("Shift+F2") );
    connect(act, SIGNAL(triggered()), c, SLOT(editNotes()));
    c->addAction(act);

    act = new QAction(c);
    act->setShortcut( QString("F2") );
    connect(act, SIGNAL(triggered()), c, SLOT(editSelected()));
    c->addAction(act);

    // Connect signals from theme buttons.
    foreach (QPushButton *button, ui->scrollAreaTheme->findChildren<QPushButton *>()) {
        if (button->objectName().endsWith("Font"))
            connect(button, SIGNAL(clicked()), SLOT(onFontButtonClicked()));
        else if (button->objectName().startsWith("pushButtonColor"))
            connect(button, SIGNAL(clicked()), SLOT(onColorButtonClicked()));
    }

    initThemeOptions();
}

void ConfigTabAppearance::decorateBrowser(ClipboardBrowser *c) const
{
    QFont font;
    QPalette p;
    QColor color;

    // scrollbars
    Qt::ScrollBarPolicy scrollbarPolicy = themeValue("show_scrollbars").toBool()
            ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    c->setVerticalScrollBarPolicy(scrollbarPolicy);
    c->setHorizontalScrollBarPolicy(scrollbarPolicy);

    Theme unfocusedTheme = this->unfocusedTheme();

    // colors and font
    c->setStyleSheet(
        "ClipboardBrowser,#item,#item_child{"
          + getFontStyleSheet( themeValue("font").toString() ) +
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
          + themeStyleSheet("sel_item_css", unfocusedTheme) +
        "}"

        + getToolTipStyleSheet() +

        // Allow user to change CSS.
        "ClipboardBrowser{" + themeStyleSheet("item_css") + "}"
        "ClipboardBrowser::item:alternate{" + themeStyleSheet("alt_item_css") + "}"
        "ClipboardBrowser::item:selected{" + themeStyleSheet("sel_item_css") + "}"
    );

    // search style
    ItemDelegate *d = static_cast<ItemDelegate *>( c->itemDelegate() );
    font = themeFont("find_font");
    color = themeColor("find_bg");
    p.setColor(QPalette::Base, color);
    color = themeColor("find_fg");
    p.setColor(QPalette::Text, color);
    d->setSearchStyle(font, p);

    // editor style
    d->setSearchStyle(font, p);
    font = themeFont("edit_font");
    color = themeColor("edit_bg");
    p.setColor(QPalette::Base, color);
    color = themeColor("edit_fg");
    p.setColor(QPalette::Text, color);
    d->setEditorStyle(font, p);

    // number style
    d->setRowNumberVisibility(themeValue("show_number").toBool());
    font = themeFont("num_font");
    color = themeColor("num_fg");
    p.setColor(QPalette::Text, color);
    d->setNumberStyle(font, p);

    d->setFontAntialiasing( themeValue("font_antialiasing").toBool() );

    bool ok;
    const int itemSpacing = themeValue("item_spacing").toInt(&ok);
    c->setSpacing( ok ? itemSpacing : c->fontMetrics().lineSpacing() / 6 );

    c->invalidateItemCache();
}

void ConfigTabAppearance::decorateMainWindow(QWidget *mainWindow) const
{
    if ( !m_theme.value("style_main_window").value().toBool() ) {
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

        "#tab_tree::branch:selected"
        ",#tab_tree::item:selected"
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

void ConfigTabAppearance::decorateToolBar(QWidget *toolBar) const
{
    if ( m_theme.value("style_main_window").value().toBool() ) {
        toolBar->setStyleSheet(
            "QToolBar{" + themeStyleSheet("tool_bar_css") + "}"
            "QToolButton{" + themeStyleSheet("tool_button_css") + "}"
                    );
    } else {
        toolBar->setStyleSheet(QString());
    }
}

QString ConfigTabAppearance::getToolTipStyleSheet() const
{
    const QString fg = themeColorString("notes_fg");
    return QString("#item QToolTip, QMenu QToolTip {")
            + getFontStyleSheet( themeValue("notes_font").toString() )
            + ";background:" + themeColorString("notes_bg")
            + ";color:" + fg
            // Reseting border helps in some cases to set correct backgroung color.
            + ";border:1px solid " + fg
            + ";" + themeStyleSheet("notes_css")
            + "}";
}

QString ConfigTabAppearance::getNotificationStyleSheet() const
{
    // Notification style sheet.
    QColor notificationBg = themeColor("notification_bg");
    // Notification opacity should be set with NotificationDaemon::setNotificationOpacity().
    notificationBg.setAlpha(255);

    return "Notification{"
           "background:" + serializeColor(notificationBg) + ";"
           "}"
           "Notification QWidget{"
           "color:" + themeColorString("notification_fg") + ";"
           + getFontStyleSheet( themeValue("notification_font").toString() ) +
           "}"
           ;
}

void ConfigTabAppearance::showEvent(QShowEvent *event)
{
    updateThemes();
    updateStyle();
    ui->scrollAreaTheme->setMinimumWidth( ui->scrollAreaThemeContents->minimumSizeHint().width()
                                          + ui->scrollAreaTheme->verticalScrollBar()->width() + 8);
    QWidget::showEvent(event);
}

ConfigTabAppearance::~ConfigTabAppearance()
{
    delete ui;
}

void ConfigTabAppearance::loadTheme(QSettings &settings)
{
    resetTheme();
    updateTheme(settings, &m_theme);
    updateStyle();
}

void ConfigTabAppearance::saveTheme(QSettings &settings) const
{
    QStringList keys = m_theme.keys();
    keys.sort();

    foreach (const QString &key, keys)
        settings.setValue( key, themeValue(key) );
}

QVariant ConfigTabAppearance::themeValue(const QString &name) const
{
    return themeValue(name, m_theme);
}

void ConfigTabAppearance::onFontButtonClicked()
{
    Q_ASSERT(sender() != NULL);
    fontButtonClicked(sender());
}

void ConfigTabAppearance::onColorButtonClicked()
{
    Q_ASSERT(sender() != NULL);
    colorButtonClicked(sender());
}

void ConfigTabAppearance::on_pushButtonLoadTheme_clicked()
{
    const QString filename = QFileDialog::getOpenFileName(this, tr("Open Theme File"),
                                                          defaultUserThemePath(), QString("*.ini"));
    if ( !filename.isNull() ) {
        QSettings settings(filename, QSettings::IniFormat);
        loadTheme(settings);
    }

    updateThemes();
}

void ConfigTabAppearance::on_pushButtonSaveTheme_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Theme File As"),
                                                    defaultUserThemePath(), QString("*.ini"));
    if ( !filename.isNull() ) {
        if ( !filename.endsWith(".ini") )
            filename.append(".ini");
        QSettings settings(filename, QSettings::IniFormat);
        saveTheme(settings);
        settings.sync();
    }

    updateThemes();
}

void ConfigTabAppearance::on_pushButtonResetTheme_clicked()
{
    resetTheme();
    updateStyle();
}

void ConfigTabAppearance::on_pushButtonEditTheme_clicked()
{
    if (m_editor.isEmpty()) {
        QMessageBox::warning( this, tr("No External Editor"),
                              tr("Set external editor command first!") );
        return;
    }

    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile) )
        return;

    {
        QSettings settings(tmpfile.fileName(), QSettings::IniFormat);
        saveTheme(settings);
        settings.sync();
    }

    QByteArray data = readTemporaryFileContent(tmpfile);
    // keep ini file user friendly
    data.replace("\\n",
#ifdef Q_OS_WIN
                 "\r\n"
#else
                 "\n"
#endif
                 );

    ItemEditor *editor = new ItemEditor(data, COPYQ_MIME_PREFIX "theme", m_editor, this);

    connect( editor, SIGNAL(fileModified(QByteArray,QString)),
             this, SLOT(onThemeModified(QByteArray)) );

    connect( editor, SIGNAL(closed(QObject *)),
             editor, SLOT(deleteLater()) );

    if ( !editor->start() )
        delete editor;
}

void ConfigTabAppearance::on_checkBoxShowNumber_stateChanged(int)
{
    decoratePreview();
}

void ConfigTabAppearance::on_checkBoxScrollbars_stateChanged(int)
{
    decoratePreview();
}

void ConfigTabAppearance::on_checkBoxAntialias_stateChanged(int)
{
    updateFontButtons();
    decoratePreview();
}

void ConfigTabAppearance::on_comboBoxThemes_activated(const QString &text)
{
    if ( text.isEmpty() )
        return;

    QString fileName = defaultUserThemePath() + "/" + text + ".ini";
    if ( !QFile(fileName).exists() ) {
        fileName = COPYQ_THEME_PREFIX;
        if ( fileName.isEmpty() || !QFile(fileName).exists() )
            return;
        fileName.append("/" + text + ".ini");
    }

    QSettings settings(fileName, QSettings::IniFormat);
    loadTheme(settings);
}

void ConfigTabAppearance::onThemeModified(const QByteArray &bytes)
{
    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile) )
        return;

    if ( !tmpfile.open() )
        return;

    tmpfile.write(bytes);
    tmpfile.flush();

    QSettings settings(tmpfile.fileName(), QSettings::IniFormat);
    loadTheme(settings);
}

void ConfigTabAppearance::updateTheme(QSettings &settings, QHash<QString, Option> *theme)
{
    foreach ( const QString &key, theme->keys() ) {
        if ( settings.contains(key) ) {
            QVariant value = settings.value(key);
            if ( value.isValid() )
                (*theme)[key].setValue(value);
        }
    }
}

void ConfigTabAppearance::updateThemes()
{
    // Add themes in combo box.
    ui->comboBoxThemes->clear();
    ui->comboBoxThemes->addItem(QString());

    const QStringList nameFilters("*.ini");
    const QDir::Filters filters = QDir::Files | QDir::Readable;

    QDir themesDir( defaultUserThemePath() );
    if ( themesDir.mkpath(".") ) {
        foreach ( const QFileInfo &fileInfo,
                  themesDir.entryInfoList(nameFilters, filters, QDir::Name) )
        {
            const QIcon icon = createThemeIcon( themesDir.absoluteFilePath(fileInfo.fileName()) );
            ui->comboBoxThemes->addItem( icon, fileInfo.baseName() );
        }
    }

    const QString themesPath(COPYQ_THEME_PREFIX);
    if ( !themesPath.isEmpty() ) {
        QDir dir(themesPath);
        foreach ( const QFileInfo &fileInfo,
                  dir.entryList(nameFilters, filters, QDir::Name) )
        {
            const QString name = fileInfo.baseName();
            if ( ui->comboBoxThemes->findText(name) == -1 ) {
                const QIcon icon = createThemeIcon( dir.absoluteFilePath(fileInfo.fileName()) );
                ui->comboBoxThemes->addItem(icon, name);
            }
        }
    }
}

void ConfigTabAppearance::updateStyle()
{
    if ( !isVisible() )
        return;

    updateColorButtons();
    updateFontButtons();
    decoratePreview();
}

void ConfigTabAppearance::fontButtonClicked(QObject *button)
{
    QFont font = themeFontFromString( button->property("VALUE").toString() );
    QFontDialog dialog(font, this);
    if ( dialog.exec() == QDialog::Accepted ) {
        font = dialog.selectedFont();
        button->setProperty( "VALUE", font.toString() );
        decoratePreview();
        updateFontButtons();
    }
}

void ConfigTabAppearance::colorButtonClicked(QObject *button)
{
    QColor color = evalColor( button->property("VALUE").toString(), m_theme );
    QColorDialog dialog(this);
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(color);

    if ( dialog.exec() == QDialog::Accepted ) {
        color = dialog.selectedColor();
        button->setProperty( "VALUE", serializeColor(color) );
        decoratePreview();

        QPixmap pix(16, 16);
        pix.fill(color);
        button->setProperty("icon", QIcon(pix));

        updateFontButtons();
    }
}

void ConfigTabAppearance::updateColorButtons()
{
    if ( !isVisible() )
        return;

    /* color indicating icons for color buttons */
    QSize iconSize(16, 16);
    QPixmap pix(iconSize);

    QList<QPushButton *> buttons =
            ui->scrollAreaTheme->findChildren<QPushButton *>(QRegExp("^pushButtonColor"));

    foreach (QPushButton *button, buttons) {
        QColor color = evalColor( button->property("VALUE").toString(), m_theme );
        pix.fill(color);
        button->setIcon(pix);
        button->setIconSize(iconSize);
    }
}

void ConfigTabAppearance::updateFontButtons()
{
    if ( !isVisible() )
        return;

    QSize iconSize(32, 16);
    QPixmap pix(iconSize);

    QRegExp re("^pushButton(.*)Font$");
    QList<QPushButton *> buttons = ui->scrollAreaTheme->findChildren<QPushButton *>(re);

    foreach (QPushButton *button, buttons) {
        if ( re.indexIn(button->objectName()) == -1 )
            Q_ASSERT(false);

        const QString colorButtonName = "pushButtonColor" + re.cap(1);

        QPushButton *buttonFg = ui->scrollAreaTheme->findChild<QPushButton *>(colorButtonName + "Fg");
        QColor colorFg = (buttonFg == NULL) ? themeColor("fg")
                                            : evalColor( buttonFg->property("VALUE").toString(), m_theme );

        QPushButton *buttonBg = ui->scrollAreaTheme->findChild<QPushButton *>(colorButtonName + "Bg");
        QColor colorBg = (buttonBg == NULL) ? themeColor("bg")
                                            : evalColor( buttonBg->property("VALUE").toString(), m_theme );

        pix.fill(colorBg);

        QPainter painter(&pix);
        painter.setPen(colorFg);

        QFont font = themeFontFromString( button->property("VALUE").toString() );
        painter.setFont(font);
        painter.drawText( QRect(0, 0, iconSize.width(), iconSize.height()), Qt::AlignCenter,
                          tr("Abc", "Preview text for font settings in appearance dialog") );

        button->setIcon(pix);
        button->setIconSize(iconSize);
    }
}

QFont ConfigTabAppearance::themeFontFromString(const QString &fontString) const
{
    QFont font;
    font.fromString(fontString);
    if ( !themeValue("font_antialiasing").toBool() )
        font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QColor ConfigTabAppearance::themeColor(const QString &name) const
{
    return themeColor(name, m_theme);
}

QFont ConfigTabAppearance::themeFont(const QString &name) const
{
    return themeFontFromString( themeValue(name).toString() );
}

QString ConfigTabAppearance::themeColorString(const QString &name) const
{
    return serializeColor( themeColor(name) );
}

QString ConfigTabAppearance::themeStyleSheet(const QString &name) const
{
    return themeStyleSheet(name, m_theme);
}

QString ConfigTabAppearance::themeStyleSheet(const QString &name, const ConfigTabAppearance::Theme &theme) const
{
    QString css = themeValue(name).toString();
    int i = 0;

    forever {
        i = css.indexOf("${", i);
        if (i == -1)
            break;
        int j = css.indexOf('}', i + 2);
        if (j == -1)
            break;

        const QString var = css.mid(i + 2, j - i - 2);

        const QString colorName = serializeColor( evalColor(var, theme) );
        css.replace(i, j - i + 1, colorName);
        i += colorName.size();
    }

    return css;
}

ConfigTabAppearance::Theme ConfigTabAppearance::unfocusedTheme() const
{
    QColor bg = themeColor("bg");
    QColor unfocusedSelectedBg = themeColor("sel_bg");
    unfocusedSelectedBg.setRgb(
                (bg.red() + unfocusedSelectedBg.red()) / 2,
                (bg.green() + unfocusedSelectedBg.green()) / 2,
                (bg.blue() + unfocusedSelectedBg.blue()) / 2
                );
    QHash<QString, Option> unfocusedTheme = m_theme;
    unfocusedTheme["sel_bg"] = Option(serializeColor(unfocusedSelectedBg));
    return unfocusedTheme;
}

void ConfigTabAppearance::initThemeOptions()
{
    resetTheme();

    m_theme["use_system_icons"] = Option(false, "checked", ui->checkBoxSystemIcons);
    m_theme["font_antialiasing"] = Option(true, "checked", ui->checkBoxAntialias);
    m_theme["style_main_window"] = Option(false, "checked", ui->checkBoxStyleMainWindow);
}

void ConfigTabAppearance::resetTheme()
{
    QString name;
    QPalette p;
    name = serializeColor( p.color(QPalette::Base) );
    m_theme["bg"]          = Option(name, "VALUE", ui->pushButtonColorBg);
    m_theme["edit_bg"]     = Option(name, "VALUE", ui->pushButtonColorEditorBg);
    name = serializeColor( p.color(QPalette::Text) );
    m_theme["fg"]          = Option(name, "VALUE", ui->pushButtonColorFg);
    m_theme["edit_fg"]     = Option(name, "VALUE", ui->pushButtonColorEditorFg);
    name = serializeColor( p.color(QPalette::Text).lighter(400) );
    m_theme["num_fg"]      = Option(name, "VALUE", ui->pushButtonColorNumberFg);
    name = serializeColor( p.color(QPalette::AlternateBase) );
    m_theme["alt_bg"]      = Option(name, "VALUE", ui->pushButtonColorAltBg);
    name = serializeColor( p.color(QPalette::Highlight) );
    m_theme["sel_bg"]      = Option(name, "VALUE", ui->pushButtonColorSelBg);
    name = serializeColor( p.color(QPalette::HighlightedText) );
    m_theme["sel_fg"]      = Option(name, "VALUE", ui->pushButtonColorSelFg);
    m_theme["find_bg"]     = Option("#ff0", "VALUE", ui->pushButtonColorFoundBg);
    m_theme["find_fg"]     = Option("#000", "VALUE", ui->pushButtonColorFoundFg);
    name = serializeColor( p.color(QPalette::ToolTipBase) );
    m_theme["notes_bg"]  = Option(name, "VALUE", ui->pushButtonColorNotesBg);
    name = serializeColor( p.color(QPalette::ToolTipText) );
    m_theme["notes_fg"]  = Option(name, "VALUE", ui->pushButtonColorNotesFg);
    m_theme["notification_bg"]  = Option("#333", "VALUE", ui->pushButtonColorNotificationBg);
    m_theme["notification_fg"]  = Option("#ddd", "VALUE", ui->pushButtonColorNotificationFg);

    m_theme["font"]        = Option("", "VALUE", ui->pushButtonFont);
    m_theme["edit_font"]   = Option("", "VALUE", ui->pushButtonEditorFont);
    m_theme["find_font"]   = Option("", "VALUE", ui->pushButtonFoundFont);
    m_theme["num_font"]    = Option("", "VALUE", ui->pushButtonNumberFont);
    m_theme["notes_font"]  = Option("", "VALUE", ui->pushButtonNotesFont);
    m_theme["notification_font"]  = Option("", "VALUE", ui->pushButtonNotificationFont);
    m_theme["show_number"] = Option(true, "checked", ui->checkBoxShowNumber);
    m_theme["show_scrollbars"] = Option(true, "checked", ui->checkBoxScrollbars);

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
}

QString ConfigTabAppearance::defaultUserThemePath() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    return QDir::cleanPath(settings.fileName() + "/../themes");
}

QVariant ConfigTabAppearance::themeValue(const QString &name, const QHash<QString, Option> &theme) const
{
    return theme[name].value();
}

QColor ConfigTabAppearance::themeColor(const QString &name, const QHash<QString, Option> &theme) const
{
    return evalColor( themeValue(name, theme).toString(), theme );
}

QIcon ConfigTabAppearance::createThemeIcon(const QString &fileName)
{
    QHash<QString, Option> theme;
    foreach (const QString &key, m_theme.keys())
        theme[key].setValue( m_theme[key].value() );

    QSettings settings(fileName, QSettings::IniFormat);
    updateTheme(settings, &theme);

    QPixmap pix(16, 16);
    pix.fill(Qt::black);

    QPainter p(&pix);

    QRect rect(1, 1, 14, 5);
    p.setPen(Qt::NoPen);
    p.setBrush( themeColor("sel_bg", theme) );
    p.drawRect(rect);

    rect.translate(0, 5);
    p.setBrush( themeColor("bg", theme) );
    p.drawRect(rect);

    rect.translate(0, 5);
    p.setBrush( themeColor("alt_bg", theme) );
    p.drawRect(rect);

    QLine line;

    line = QLine(2, 3, 14, 3);
    QPen pen;
    p.setOpacity(0.6);

    pen.setColor( themeColor("sel_fg", theme) );
    pen.setDashPattern(QVector<qreal>() << 2 << 1 << 1 << 1 << 3 << 1 << 2 << 10);
    p.setPen(pen);
    p.drawLine(line);

    line.translate(0, 5);
    pen.setColor( themeColor("fg", theme) );
    pen.setDashPattern(QVector<qreal>() << 2 << 1 << 4 << 10);
    p.setPen(pen);
    p.drawLine(line);

    line.translate(0, 5);
    pen.setDashPattern(QVector<qreal>() << 3 << 1 << 2 << 1);
    p.setPen(pen);
    p.drawLine(line);

    return pix;
}

void ConfigTabAppearance::decoratePreview()
{
    if ( isVisible() )
        decorateBrowser(ui->clipboardBrowserPreview);
}

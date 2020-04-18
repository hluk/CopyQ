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

#include "gui/configtabappearance.h"
#include "ui_configtabappearance.h"

#include "common/contenttype.h"
#include "common/display.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "common/temporarysettings.h"
#include "common/temporaryfile.h"
#include "common/textdata.h"
#include "gui/clipboardbrowser.h"
#include "gui/clipboardbrowsershared.h"
#include "gui/iconfont.h"
#include "gui/pixelratio.h"
#include "gui/theme.h"
#include "item/itemeditor.h"
#include "item/itemdelegate.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QColorDialog>
#include <QFileDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryFile>

ConfigTabAppearance::ConfigTabAppearance(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigTabAppearance)
    , m_theme(ui)
    , m_editor()
{
    ui->setupUi(this);

    connect(ui->pushButtonLoadTheme, &QPushButton::clicked,
            this, &ConfigTabAppearance::onPushButtonLoadThemeClicked);
    connect(ui->pushButtonSaveTheme, &QPushButton::clicked,
            this, &ConfigTabAppearance::onPushButtonSaveThemeClicked);
    connect(ui->pushButtonResetTheme, &QPushButton::clicked,
            this, &ConfigTabAppearance::onPushButtonResetThemeClicked);
    connect(ui->pushButtonEditTheme, &QPushButton::clicked,
            this, &ConfigTabAppearance::onPushButtonEditThemeClicked);

    connect(ui->checkBoxShowNumber, &QCheckBox::stateChanged,
            this, &ConfigTabAppearance::onCheckBoxShowNumberStateChanged);
    connect(ui->checkBoxScrollbars, &QCheckBox::stateChanged,
            this, &ConfigTabAppearance::onCheckBoxScrollbarsStateChanged);
    connect(ui->checkBoxAntialias, &QCheckBox::stateChanged,
            this, &ConfigTabAppearance::onCheckBoxAntialiasStateChanged);

    connect(ui->comboBoxThemes, static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::activated),
            this, &ConfigTabAppearance::onComboBoxThemesActivated);

    // Connect signals from theme buttons.
    for (auto button : ui->scrollAreaTheme->findChildren<QPushButton *>()) {
        if (button->objectName().endsWith("Font"))
            connect(button, &QPushButton::clicked, this, &ConfigTabAppearance::onFontButtonClicked);
        else if (button->objectName().startsWith("pushButtonColor"))
            connect(button, &QPushButton::clicked, this, &ConfigTabAppearance::onColorButtonClicked);
    }

    m_theme.resetTheme();
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

void ConfigTabAppearance::loadTheme(const QSettings &settings)
{
    m_theme.loadTheme(settings);
    updateStyle();
}

void ConfigTabAppearance::saveTheme(QSettings *settings)
{
    m_theme.saveTheme(settings);
    settings->sync();
    updateThemes();
}

void ConfigTabAppearance::createPreview(ItemFactory *itemFactory)
{
    m_itemFactory = itemFactory;
    decoratePreview();
}

void ConfigTabAppearance::onFontButtonClicked()
{
    Q_ASSERT(sender() != nullptr);
    fontButtonClicked(sender());
}

void ConfigTabAppearance::onColorButtonClicked()
{
    Q_ASSERT(sender() != nullptr);
    colorButtonClicked(sender());
}

void ConfigTabAppearance::onPushButtonLoadThemeClicked()
{
    const QString filename = QFileDialog::getOpenFileName(this, tr("Open Theme File"),
                                                          defaultUserThemePath(), QString("*.ini"));
    if ( !filename.isNull() ) {
        QSettings settings(filename, QSettings::IniFormat);
        loadTheme(settings);
    }
}

void ConfigTabAppearance::onPushButtonSaveThemeClicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save Theme File As"),
                                                    defaultUserThemePath(), QString("*.ini"));
    if ( !filename.isNull() ) {
        if ( !filename.endsWith(".ini") )
            filename.append(".ini");
        QSettings settings(filename, QSettings::IniFormat);
        saveTheme(&settings);
    }
}

void ConfigTabAppearance::onPushButtonResetThemeClicked()
{
    m_theme.resetTheme();
    updateStyle();
}

void ConfigTabAppearance::onPushButtonEditThemeClicked()
{
    if (m_editor.isEmpty()) {
        QMessageBox::warning( this, tr("No External Editor"),
                              tr("Set external editor command first!") );
        return;
    }

    TemporarySettings settings;
    saveTheme(settings.settings());

    QByteArray data = settings.content();
    // keep ini file user friendly
    data.replace("\\n",
#ifdef Q_OS_WIN
                 "\r\n"
#else
                 "\n"
#endif
                 );

    ItemEditor *editor = new ItemEditor(data, COPYQ_MIME_PREFIX "theme", m_editor, this);

    connect( editor, &ItemEditor::fileModified,
             this, &ConfigTabAppearance::onThemeModified );

    connect( editor, &ItemEditor::closed,
             editor, &QObject::deleteLater );

    if ( !editor->start() )
        delete editor;
}

void ConfigTabAppearance::onCheckBoxShowNumberStateChanged(int)
{
    decoratePreview();
}

void ConfigTabAppearance::onCheckBoxScrollbarsStateChanged(int)
{
    decoratePreview();
}

void ConfigTabAppearance::onCheckBoxAntialiasStateChanged(int)
{
    updateFontButtons();
    decoratePreview();
}

void ConfigTabAppearance::onComboBoxThemesActivated(const QString &text)
{
    if ( text.isEmpty() )
        return;

    const QString fileName = findThemeFile(text + ".ini");
    QSettings settings(fileName, QSettings::IniFormat);
    loadTheme(settings);
}

void ConfigTabAppearance::onThemeModified(const QByteArray &bytes)
{
    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile, ".ini") )
        return;

    tmpfile.write(bytes);
    tmpfile.flush();

    QSettings settings(tmpfile.fileName(), QSettings::IniFormat);
    loadTheme(settings);
}

void ConfigTabAppearance::updateThemes()
{
    // Add themes in combo box.
    ui->comboBoxThemes->clear();
    ui->comboBoxThemes->addItem(QString());

    for (const QString &path : themePaths())
        addThemes(path);
}

void ConfigTabAppearance::addThemes(const QString &path)
{
    const QDir::Filters filters = QDir::Files | QDir::Readable;
    const QStringList nameFilters("*.ini");

    QDir dir(path);
    for ( const auto &fileInfo :
              dir.entryInfoList(nameFilters, filters, QDir::Name) )
    {
        const QString name = fileInfo.baseName();
        if ( ui->comboBoxThemes->findText(name) == -1 ) {
            const QIcon icon = createThemeIcon( dir.absoluteFilePath(fileInfo.fileName()) );
            ui->comboBoxThemes->addItem(icon, name);
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
    QFont font = m_theme.themeFontFromString( button->property("VALUE").toString() );
    QFontDialog dialog(this);
    dialog.setOption(QFontDialog::DontUseNativeDialog);
    // WORKAROUND: DontUseNativeDialog must be set before current font (QTBUG-79637).
    dialog.setCurrentFont(font);
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
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    dialog.setCurrentColor(color);

    if ( dialog.exec() == QDialog::Accepted ) {
        color = dialog.selectedColor();
        button->setProperty( "VALUE", serializeColor(color) );
        decoratePreview();

        const QSize iconSize = button->property("iconSize").toSize();
        QPixmap pix(iconSize);
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
    const QSize iconSize = ui->pushButtonColorFg->iconSize();
    QPixmap pix(iconSize);

    QList<QPushButton *> buttons =
            ui->scrollAreaTheme->findChildren<QPushButton *>(QRegularExpression("^pushButtonColor"));

    for (auto button : buttons) {
        QColor color = evalColor( button->property("VALUE").toString(), m_theme );
        pix.fill(color);
        button->setIcon(pix);
        button->setIconSize(pix.size());
    }
}

void ConfigTabAppearance::updateFontButtons()
{
    if ( !isVisible() )
        return;

    const int iconExtent = pointsToPixels(12);
    const QSize iconSize(iconExtent * 2, iconExtent);

    const auto ratio = pixelRatio(ui->scrollAreaTheme);
    QPixmap pix(iconSize * ratio);
    pix.setDevicePixelRatio(ratio);

    const QRect textRect( QPoint(0, 0), iconSize );

    const QRegularExpression re("^pushButton(.*)Font$");
    QList<QPushButton *> buttons = ui->scrollAreaTheme->findChildren<QPushButton *>(re);

    for (auto button : buttons) {
        const auto m = re.match(button->objectName());
        Q_ASSERT(m.hasMatch());

        const QString colorButtonName = "pushButtonColor" + m.captured(1);

        QPushButton *buttonFg = ui->scrollAreaTheme->findChild<QPushButton *>(colorButtonName + "Fg");
        QColor colorFg = (buttonFg == nullptr) ? m_theme.color("fg")
                                            : evalColor( buttonFg->property("VALUE").toString(), m_theme );

        QPushButton *buttonBg = ui->scrollAreaTheme->findChild<QPushButton *>(colorButtonName + "Bg");
        QColor colorBg = (buttonBg == nullptr) ? m_theme.color("bg")
                                            : evalColor( buttonBg->property("VALUE").toString(), m_theme );

        pix.fill(colorBg);

        QPainter painter(&pix);
        painter.setPen(colorFg);

        QFont font = m_theme.themeFontFromString( button->property("VALUE").toString() );
        painter.setFont(font);
        painter.drawText( textRect, Qt::AlignCenter,
                          tr("Abc", "Preview text for font settings in appearance dialog") );

        button->setIcon(pix);
        button->setIconSize(iconSize);
    }
}

QIcon ConfigTabAppearance::createThemeIcon(const QString &fileName)
{
    QSettings settings(fileName, QSettings::IniFormat);
    Theme theme(settings);

    QPixmap pix(16, 16);
    pix.fill(Qt::black);

    QPainter p(&pix);

    QRect rect(1, 1, 14, 5);
    p.setPen(Qt::NoPen);
    p.setBrush( theme.color("sel_bg") );
    p.drawRect(rect);

    rect.translate(0, 5);
    p.setBrush( theme.color("bg") );
    p.drawRect(rect);

    rect.translate(0, 5);
    p.setBrush( theme.color("alt_bg") );
    p.drawRect(rect);

    QLine line;

    line = QLine(2, 3, 14, 3);
    QPen pen;
    p.setOpacity(0.6);

    pen.setColor( theme.color("sel_fg") );
    pen.setDashPattern(QVector<qreal>() << 2 << 1 << 1 << 1 << 3 << 1 << 2 << 10);
    p.setPen(pen);
    p.drawLine(line);

    line.translate(0, 5);
    pen.setColor( theme.color("fg") );
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
    if ( !m_itemFactory || !isVisible() )
        return;

    if (m_preview) {
        delete m_preview;
        m_preview = nullptr;
    }

    m_theme.updateTheme();

    const auto sharedData = std::make_shared<ClipboardBrowserShared>();
    sharedData->itemFactory = m_itemFactory;
    sharedData->theme = m_theme;

    auto c = new ClipboardBrowser(QString(), sharedData, this);
    m_preview = c;
    m_theme.decorateBrowser(c);

    ui->browserParentLayout->addWidget(c);

    const QString searchFor = tr("item", "Search expression in preview in Appearance tab.");

    c->add( tr("Search string is %1.").arg(quoteString(searchFor)) );
    c->add( tr("Select an item and\npress F2 to edit.") );
    for (int i = 1; i <= 20; ++i)
        c->add( tr("Example item %1").arg(i), -1 );

    QAbstractItemModel *model = c->model();
    QModelIndex index = model->index(0, 0);
    QVariantMap dataMap;
    dataMap.insert( mimeItemNotes, tr("Some random notes (Shift+F2 to edit)").toUtf8() );
    model->setData(index, dataMap, contentType::updateData);

    // Highlight found text but don't filter out any items.
    c->filterItems( QRegularExpression(QString("^|") + searchFor, QRegularExpression::CaseInsensitiveOption) );

    QAction *act;

    act = new QAction(c);
    act->setShortcut( QString("Shift+F2") );
    connect(act, &QAction::triggered, c, &ClipboardBrowser::editNotes);
    c->addAction(act);

    act = new QAction(c);
    act->setShortcut( QString("F2") );
    connect(act, &QAction::triggered, c, &ClipboardBrowser::editSelected);
    c->addAction(act);
}

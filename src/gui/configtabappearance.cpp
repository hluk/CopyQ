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

#include "gui/configtabappearance.h"
#include "ui_configtabappearance.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/option.h"
#include "gui/clipboardbrowser.h"
#include "gui/iconfont.h"
#include "gui/theme.h"
#include "item/itemeditor.h"
#include "item/itemdelegate.h"

#include <QAbstractScrollArea>
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

ConfigTabAppearance::ConfigTabAppearance(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigTabAppearance)
    , m_theme(ui)
    , m_editor()
    , m_preview(NULL)
{
    ui->setupUi(this);

    // Connect signals from theme buttons.
    foreach (QPushButton *button, ui->scrollAreaTheme->findChildren<QPushButton *>()) {
        if (button->objectName().endsWith("Font"))
            connect(button, SIGNAL(clicked()), SLOT(onFontButtonClicked()));
        else if (button->objectName().startsWith("pushButtonColor"))
            connect(button, SIGNAL(clicked()), SLOT(onColorButtonClicked()));
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

void ConfigTabAppearance::loadTheme(QSettings &settings)
{
    m_theme.loadTheme(settings);
    updateStyle();
}

void ConfigTabAppearance::saveTheme(QSettings &settings)
{
    m_theme.saveTheme(settings);
    settings.sync();
    updateThemes();
}

void ConfigTabAppearance::createPreview(ItemFactory *itemFactory)
{
    if (m_preview)
        return;

    ClipboardBrowserSharedPtr sharedData(new ClipboardBrowserShared(itemFactory));
    ClipboardBrowser *c = new ClipboardBrowser(sharedData, this);
    ui->browserParentLayout->addWidget(c);
    m_preview = c;

    const QString searchFor = tr("item", "Search expression in preview in Appearance tab.");

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
    }
}

void ConfigTabAppearance::on_pushButtonResetTheme_clicked()
{
    m_theme.resetTheme();
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
    QFont font = m_theme.themeFontFromString( button->property("VALUE").toString() );
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
        QColor colorFg = (buttonFg == NULL) ? m_theme.color("fg")
                                            : evalColor( buttonFg->property("VALUE").toString(), m_theme );

        QPushButton *buttonBg = ui->scrollAreaTheme->findChild<QPushButton *>(colorButtonName + "Bg");
        QColor colorBg = (buttonBg == NULL) ? m_theme.color("bg")
                                            : evalColor( buttonBg->property("VALUE").toString(), m_theme );

        pix.fill(colorBg);

        QPainter painter(&pix);
        painter.setPen(colorFg);

        QFont font = m_theme.themeFontFromString( button->property("VALUE").toString() );
        painter.setFont(font);
        painter.drawText( QRect(0, 0, iconSize.width(), iconSize.height()), Qt::AlignCenter,
                          tr("Abc", "Preview text for font settings in appearance dialog") );

        button->setIcon(pix);
        button->setIconSize(iconSize);
    }
}

QString ConfigTabAppearance::defaultUserThemePath() const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    return QDir::cleanPath(settings.fileName() + "/../themes");
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
    if ( m_preview && isVisible() )
        m_preview->decorate(m_theme);
}

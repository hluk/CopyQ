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

#include "clipboarddialog.h"
#include "ui_clipboarddialog.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "common/timer.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"

#include <QBuffer>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMovie>
#include <QScrollBar>
#include <QUrl>

namespace {

// Limit number of characters to load at once - performance reasons.
constexpr int batchLoadCharacters = 4096;
constexpr int priorityHigh = 100;
constexpr int priorityMedium = priorityHigh / 2;
constexpr int priorityLow = priorityMedium / 2;
constexpr int priorityLower = priorityLow / 2;
constexpr int priorityLowest = 0;

QVariant cloneClipboardData(const QString &format)
{
    const QMimeData *clipData = clipboardData();
    if (!clipData)
        return QVariant();

    return cloneData(*clipData, QStringList(format)).value(format);
}

int formatSortPriority(const QString &format)
{
    if (format == mimeText)
        return priorityHigh + 2;

    if (format == mimeHtml)
        return priorityHigh + 1;

    if ( format.startsWith("text/") )
        return priorityHigh;

    if ( format.startsWith("application/") )
        return priorityLow;

    if ( format.contains("/x-") )
        return priorityLower;

    if ( format.isEmpty() || format[0].isUpper() )
        return priorityLowest;

    return priorityMedium;
}

} // namespace

ClipboardDialog::ClipboardDialog(QWidget *parent)
    : QDialog(parent)
{
    init();

    const QMimeData *clipData = clipboardData();
    if (clipData) {
        m_dataFromClipboard = true;
        for (const auto &format : clipData->formats()) {
            m_data.insert(format, QVariant());
        }
        setData(m_data);
    }
}

ClipboardDialog::ClipboardDialog(
        const QPersistentModelIndex &index, QAbstractItemModel *model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_index(index)
{
    init();

    setWindowTitle( tr("Item Content") );
    connect( m_model.data(), &QAbstractItemModel::dataChanged,
             this, &ClipboardDialog::onDataChanged );
    onDataChanged(m_index, m_index);
}

ClipboardDialog::~ClipboardDialog()
{
    delete ui;
}

void ClipboardDialog::onListWidgetFormatsCurrentItemChanged(
        QListWidgetItem *current, QListWidgetItem *)
{
    ui->actionRemove_Format->setEnabled(current != nullptr);

    const QString mime = current ? current->text() : QString();
    const bool hasImage = mime.startsWith(QString("image")) ;
    const QByteArray animationFormat =
            QString(mime).remove(QRegularExpression("^image/")).toUtf8();
    const bool hasAnimation = QMovie::supportedFormats().contains(animationFormat);

    ui->textEdit->clear();
    ui->textEdit->setVisible(!hasImage);
    ui->scrollAreaImage->setVisible(hasImage);

    if (hasImage)
        ui->labelContent->setBuddy(ui->scrollAreaImage);
    else
        ui->labelContent->setBuddy(ui->textEdit);

    QVariant value = m_data.value(mime);
    if ( m_dataFromClipboard && !value.isValid() )
        value = m_data[mime] = cloneClipboardData(mime).toByteArray();

    const QByteArray bytes = value.toByteArray();

    m_timerTextLoad.stop();

    if (hasAnimation) {
        if (m_animation)
            m_animation->deleteLater();

        if (m_animationBuffer)
            m_animationBuffer->deleteLater();

        m_animationBuffer = new QBuffer(this);
        m_animationBuffer->open(QIODevice::ReadWrite);
        m_animationBuffer->write(bytes);
        m_animationBuffer->seek(0);

        m_animation = new QMovie(this);
        m_animation->setDevice(m_animationBuffer);
        m_animation->setFormat(animationFormat);
        ui->labelImage->setMovie(m_animation);
        m_animation->start();
    } else if (hasImage) {
        QPixmap pix;
        pix.loadFromData( bytes, mime.toLatin1() );
        ui->labelImage->setPixmap(pix);
    } else {
        m_textToShow = dataToText(bytes, mime);
        addText();
    }

    if (current) {
        ui->labelProperties->setText(
                tr("<strong>Size:</strong> %1 bytes", "Size of clipboard/item data in bytes")
                .arg(bytes.size()) );
    } else {
        ui->labelProperties->setText(QString());
    }
}

void ClipboardDialog::onActionRemoveFormatTriggered()
{
     QListWidgetItem *item = ui->listWidgetFormats->currentItem();
     if (item) {
         const QString mimeToRemove = item->text();
         m_data.remove(mimeToRemove);
         delete item;

         if (m_model)
             m_model->setData(m_index, mimeToRemove, contentType::removeFormats);
         else
             emit changeClipboard(m_data);
     }
}

void ClipboardDialog::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (m_index.isValid()
            && topLeft.row() <= m_index.row()
            && m_index.row() <= bottomRight.row())
    {
        setData( m_index.data(contentType::data).toMap() );
    }
}

void ClipboardDialog::addText()
{
    const int scrollValue = ui->textEdit->verticalScrollBar()->value();

    ui->textEdit->appendPlainText( m_textToShow.left(batchLoadCharacters) );
    m_textToShow.remove(0, batchLoadCharacters);

    if ( !m_textToShow.isEmpty() )
        m_timerTextLoad.start();

    ui->textEdit->verticalScrollBar()->setValue(scrollValue);
}

void ClipboardDialog::init()
{
    Q_ASSERT(!ui);

    ui = new Ui::ClipboardDialog;
    ui->setupUi(this);

    connect(ui->listWidgetFormats, &QListWidget::currentItemChanged,
            this, &ClipboardDialog::onListWidgetFormatsCurrentItemChanged);
    connect(ui->actionRemove_Format, &QAction::triggered,
            this, &ClipboardDialog::onActionRemoveFormatTriggered);

    setWindowIcon(appIcon());

    ui->horizontalLayout->setStretchFactor(1, 1);
    ui->listWidgetFormats->setCurrentRow(0);

    WindowGeometryGuard::create(this);

    ui->actionRemove_Format->setIcon( getIcon("list-remove", IconTrash) );
    ui->actionRemove_Format->setShortcut(shortcutToRemove());
    ui->listWidgetFormats->addAction(ui->actionRemove_Format);

    onListWidgetFormatsCurrentItemChanged(nullptr, nullptr);
}

void ClipboardDialog::setData(const QVariantMap &data)
{
    const QString currentFormat = ui->listWidgetFormats->currentIndex().data().toString();
    ui->listWidgetFormats->clear();
    m_data = data;

    QStringList formats = data.keys();
    std::sort( std::begin(formats), std::end(formats), [](const QString &lhs, const QString &rhs) {
        const int lhsPriority = formatSortPriority(lhs);
        const int rhsPriority = formatSortPriority(rhs);
        return lhsPriority == rhsPriority ? lhs < rhs : lhsPriority > rhsPriority;
    } );
    for (const QString &format : formats) {
        ui->listWidgetFormats->addItem(format);
        if (format == currentFormat) {
            ui->listWidgetFormats->setCurrentRow(
                        ui->listWidgetFormats->count() - 1);
        }
    }

    initSingleShotTimer(&m_timerTextLoad, 10, this, &ClipboardDialog::addText);
}

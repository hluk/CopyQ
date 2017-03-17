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

#include "clipboarddialog.h"
#include "ui_clipboarddialog.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/shortcuts.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"

#include <QBuffer>
#include <QListWidgetItem>
#include <QMovie>
#include <QScrollBar>
#include <QUrl>

// Limit number of characters to load at once - performance reasons.
static const int batchLoadCharacters = 4096;

ClipboardDialog::ClipboardDialog(QWidget *parent)
    : QDialog(parent)
    , ui(nullptr)
    , m_animationBuffer(nullptr)
    , m_animation(nullptr)
{
    init();

    const QMimeData *clipData = clipboardData();
    if (clipData)
        setData( cloneData(*clipData) );
}

ClipboardDialog::ClipboardDialog(
        const QPersistentModelIndex &index, QAbstractItemModel *model, QWidget *parent)
    : QDialog(parent)
    , ui(nullptr)
    , m_model(model)
    , m_index(index)
    , m_animationBuffer(nullptr)
    , m_animation(nullptr)
{
    init();

    setWindowTitle( tr("CopyQ Item Content") );
    connect( m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             this, SLOT(onDataChanged(QModelIndex,QModelIndex)) );
    onDataChanged(m_index, m_index);
}

ClipboardDialog::~ClipboardDialog()
{
    delete ui;
}

void ClipboardDialog::on_listWidgetFormats_currentItemChanged(
        QListWidgetItem *current, QListWidgetItem *)
{
    ui->actionRemove_Format->setEnabled(current != nullptr);

    const QString mime = current ? current->text() : QString();
    const bool hasImage = mime.startsWith(QString("image")) ;
    const QByteArray animationFormat =
            QString(mime).remove(QRegExp("^image/")).toUtf8();
    const bool hasAnimation = QMovie::supportedFormats().contains(animationFormat);

    ui->textEdit->clear();
    ui->textEdit->setVisible(!hasImage);
    ui->scrollAreaImage->setVisible(hasImage);

    if (hasImage)
        ui->labelContent->setBuddy(ui->scrollAreaImage);
    else
        ui->labelContent->setBuddy(ui->textEdit);

    const QByteArray bytes = m_data.value(mime).toByteArray();

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

    ui->labelProperties->setText(
                tr("<strong>Size:</strong> %1 bytes", "Size of data in bytes").arg(bytes.size()) );
}

void ClipboardDialog::on_actionRemove_Format_triggered()
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

    setWindowIcon(appIcon());

    ui->horizontalLayout->setStretchFactor(1, 1);
    ui->listWidgetFormats->setCurrentRow(0);

    WindowGeometryGuard::create(this);

    ui->actionRemove_Format->setIcon( getIcon("list-remove", IconRemove) );
    ui->actionRemove_Format->setShortcut(shortcutToRemove());
    ui->listWidgetFormats->addAction(ui->actionRemove_Format);

    on_listWidgetFormats_currentItemChanged(nullptr, nullptr);
}

void ClipboardDialog::setData(const QVariantMap &data)
{
    const QString currentFormat = ui->listWidgetFormats->currentIndex().data().toString();
    ui->listWidgetFormats->clear();
    m_data.clear();

    // Show only data that can be displayed.
    for ( const auto &mime : data.keys() ) {
        if ( data[mime].canConvert<QByteArray>() ) {
            m_data.insert(mime, data[mime]);
            ui->listWidgetFormats->addItem(mime);
            if (mime == currentFormat) {
                ui->listWidgetFormats->setCurrentRow(
                            ui->listWidgetFormats->count() - 1);
            }
        }
    }

    initSingleShotTimer(&m_timerTextLoad, 10, this, SLOT(addText()));
}

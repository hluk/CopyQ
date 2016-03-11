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

#include "clipboarddialog.h"
#include "ui_clipboarddialog.h"

#include "common/common.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/windowgeometryguard.h"

#include <QListWidgetItem>
#include <QUrl>

ClipboardDialog::ClipboardDialog(QWidget *parent)
    : QDialog(parent)
    , ui(NULL)
{
    init();

    const QMimeData *clipData = clipboardData();
    if (clipData)
        setData( cloneData(*clipData) );
}

ClipboardDialog::ClipboardDialog(
        const QPersistentModelIndex &index, QAbstractItemModel *model, QWidget *parent)
    : QDialog(parent)
    , ui(NULL)
    , m_model(model)
    , m_index(index)
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
    ui->actionRemove_Format->setEnabled(current != NULL);

    QTextEdit *edit = ui->textEditContent;
    QString mime = current ? current->text() : QString();

    edit->clear();
    const QByteArray bytes = m_data.value(mime).toByteArray();
    if ( mime.startsWith(QString("image")) ) {
        edit->document()->addResource( QTextDocument::ImageResource,
                                       QUrl("data://1"), bytes );
        edit->setHtml( QString("<img src=\"data://1\" />") );
    } else {
        edit->setPlainText( dataToText(bytes, mime) );
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
}

void ClipboardDialog::setData(const QVariantMap &data)
{
    const QString currentFormat = ui->listWidgetFormats->currentIndex().data().toString();
    ui->listWidgetFormats->clear();
    m_data.clear();

    // Show only data that can be displayed.
    foreach ( const QString &mime, data.keys() ) {
        if ( data[mime].canConvert<QByteArray>() ) {
            m_data.insert(mime, data[mime]);
            ui->listWidgetFormats->addItem(mime);
            if (mime == currentFormat) {
                ui->listWidgetFormats->setCurrentRow(
                            ui->listWidgetFormats->count() - 1);
            }
        }
    }
}

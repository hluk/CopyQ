/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

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
#include "client_server.h"
#include <QClipboard>
#include <QMimeData>
#include <QUrl>

ClipboardDialog::ClipboardDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClipboardDialog)
    , m_data()
{
    ui->setupUi(this);

    const QMimeData *data = clipboardData();
    if (data) {
        foreach ( const QString &mime, data->formats() ) {
            if ( mime.contains("/") )
                ui->listWidgetFormats->addItem(mime);
        }
        ui->horizontalLayout->setStretchFactor(0,1);
        ui->horizontalLayout->setStretchFactor(1,3);
        ui->listWidgetFormats->setCurrentRow(0);
    }
}

ClipboardDialog::~ClipboardDialog()
{
    delete ui;
}

void ClipboardDialog::on_listWidgetFormats_currentItemChanged(
        QListWidgetItem *current, QListWidgetItem *)
{
    QTextEdit *edit = ui->textEditContent;
    QString mime = current->text();

    if ( !m_data.contains(mime) ) {
        const QMimeData *data = clipboardData();
        if (data) {
            m_data[mime] = data->data(mime);
        }
    }

    edit->clear();
    QByteArray &bytes = m_data[mime];
    if ( mime.startsWith(QString("image")) ) {
        edit->document()->addResource( QTextDocument::ImageResource,
                                       QUrl("data://1"), bytes );
        edit->setHtml( QString("<img src=\"data://1\" />") );
    } else {
        edit->setPlainText(bytes);
    }

    ui->labelProperties->setText(
                "<strong> mime: </strong>" + Qt::escape(mime) +
                "<strong> size: </strong>" + QString::number(bytes.size()) + " bytes"
                );
}

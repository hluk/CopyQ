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
#include "configurationmanager.h"
#include <QClipboard>
#include <QUrl>

static QString escapeHtml(const QString &str)
{
#if QT_VERSION >= 0x050000
    return str.toHtmlEscaped();
#else
    return Qt::escape(str);
#endif
}

ClipboardDialog::ClipboardDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClipboardDialog)
    , m_data()
{
    ui->setupUi(this);

    const QMimeData *data = clipboardData();
    if (data) {
        foreach ( const QString &mime, data->formats() ) {
            if ( mime.contains("/") ) {
                m_data.setData( mime, data->data(mime) );
                ui->listWidgetFormats->addItem(mime);
            }
        }
        ui->horizontalLayout->setStretchFactor(0,1);
        ui->horizontalLayout->setStretchFactor(1,2);
        ui->listWidgetFormats->setCurrentRow(0);
    }

    ConfigurationManager::instance()->loadGeometry(this);
}

ClipboardDialog::~ClipboardDialog()
{
    ConfigurationManager::instance()->saveGeometry(this);

    delete ui;
}

void ClipboardDialog::on_listWidgetFormats_currentItemChanged(
        QListWidgetItem *current, QListWidgetItem *)
{
    QTextEdit *edit = ui->textEditContent;
    QString mime = current->text();

    edit->clear();
    QByteArray bytes = m_data.data(mime);
    if ( mime.startsWith(QString("image")) ) {
        edit->document()->addResource( QTextDocument::ImageResource,
                                       QUrl("data://1"), bytes );
        edit->setHtml( QString("<img src=\"data://1\" />") );
    } else {
        edit->setPlainText( QString::fromLocal8Bit(bytes) );
    }

    ui->labelProperties->setText(
        tr("<strong> mime:</strong> %1 <strong>size:</strong> %2 bytes")
            .arg(escapeHtml(mime))
            .arg(QString::number(bytes.size())));
}

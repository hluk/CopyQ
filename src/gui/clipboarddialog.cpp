/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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
#include "configurationmanager.h"

#include <QListWidgetItem>
#include <QTextCodec>
#include <QUrl>

ClipboardDialog::ClipboardDialog(const QVariantMap &itemData, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClipboardDialog)
    , m_data()
{
    ui->setupUi(this);

    QVariantMap data;
    if ( itemData.isEmpty() ) {
        const QMimeData *clipData = clipboardData();
        if (clipData)
            data = cloneData(*clipData);
    } else {
        data = itemData;
    }

    foreach ( const QString &mime, data.keys() ) {
        if ( data[mime].canConvert<QByteArray>() ) {
            m_data.insert(mime, data[mime]);
            ui->listWidgetFormats->addItem(mime);
        }
    }
    ui->horizontalLayout->setStretchFactor(1, 1);
    ui->listWidgetFormats->setCurrentRow(0);

    ConfigurationManager::instance()->loadGeometry(this);

    if ( itemData.isEmpty() )
        setWindowTitle( tr("CopyQ Item Content") );
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
    const QByteArray bytes = m_data.value(mime).toByteArray();
    if ( mime.startsWith(QString("image")) ) {
        edit->document()->addResource( QTextDocument::ImageResource,
                                       QUrl("data://1"), bytes );
        edit->setHtml( QString("<img src=\"data://1\" />") );
    } else {
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        if (mime == QLatin1String("text/html"))
            codec = QTextCodec::codecForHtml(bytes, codec);
        else
            codec = QTextCodec::codecForUtfText(bytes, codec);
        edit->setPlainText( codec ? codec->toUnicode(bytes) : QString() );
    }

    ui->labelProperties->setText(
        tr("<strong> mime:</strong> %1 <strong>size:</strong> %2 bytes")
            .arg(escapeHtml(mime))
            .arg(QString::number(bytes.size())));
}

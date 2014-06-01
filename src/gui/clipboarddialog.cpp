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

#include "clipboarddialog.h"
#include "ui_clipboarddialog.h"

#include "common/common.h"
#include "common/mimetypes.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QListWidgetItem>
#include <QTextCodec>
#include <QUrl>

ClipboardDialog::ClipboardDialog(const ClipboardItemPtr &item, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClipboardDialog)
    , m_item(item)
{
    ui->setupUi(this);

    setWindowIcon( ConfigurationManager::instance()->iconFactory()->appIcon() );

    QVariantMap data;
    if ( m_item.isNull() ) {
        const QMimeData *clipData = clipboardData();
        if (clipData)
            data = cloneData(*clipData);
    } else {
        setWindowTitle( tr("CopyQ Item Content") );
        data = item->data();
    }

    // Show only data that can be displayed.
    foreach ( const QString &mime, data.keys() ) {
        if ( data[mime].canConvert<QByteArray>() ) {
            m_data.insert(mime, data[mime]);
            ui->listWidgetFormats->addItem(mime);
        }
    }

    ui->horizontalLayout->setStretchFactor(1, 1);
    ui->listWidgetFormats->setCurrentRow(0);

    ConfigurationManager::instance()->registerWindowGeometry(this);

    ui->actionRemove_Format->setIcon( getIcon("list-remove", IconRemove) );
    ui->actionRemove_Format->setShortcut(shortcutToRemove());
    ui->listWidgetFormats->addAction(ui->actionRemove_Format);
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
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        if (mime == QLatin1String("text/html"))
            codec = QTextCodec::codecForHtml(bytes, codec);
        else
            codec = QTextCodec::codecForUtfText(bytes, codec);
        edit->setPlainText( codec ? codec->toUnicode(bytes) : QString() );
    }

    ui->labelProperties->setText(
                tr("<strong>Size:</strong> %1 bytes", "Size of data in bytes").arg(bytes.size()) );
}

void ClipboardDialog::on_actionRemove_Format_triggered()
{
     QListWidgetItem *item = ui->listWidgetFormats->currentItem();
     if (item) {
         m_data.remove(item->text());
         if ( m_item.isNull() )
             emit changeClipboard(m_data);
         else
             m_item->setData(m_data);
         delete item;
     }
}

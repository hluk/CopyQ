#include "clipboarddialog.h"
#include "ui_clipboarddialog.h"
#include <QClipboard>
#include <QMimeData>
#include <QUrl>

ClipboardDialog::ClipboardDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ClipboardDialog)
{
    ui->setupUi(this);

    QClipboard *c = QApplication::clipboard();
    const QMimeData *data = c->mimeData();
    if (data) {
        foreach( const QString &mime, data->formats() ) {
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
        QClipboard *c = QApplication::clipboard();
        const QMimeData *data = c->mimeData();
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

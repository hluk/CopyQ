#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // load about document from resources
    QFile f(":/aboutdialog.html");
    if ( f.open(QIODevice::ReadOnly) ) {
        ui->textEdit->setText( QString::fromUtf8(f.readAll()) );
    }
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::showEvent(QShowEvent *)
{
    /* try to resize the dialog so that vertical scrollbar in the about
     * document is hidden
     */
    resize( width(), ui->textEdit->document()->size().height()+60 );
}

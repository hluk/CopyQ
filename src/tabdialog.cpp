#include "tabdialog.h"
#include "ui_tabdialog.h"

TabDialog::TabDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TabDialog)
{
    ui->setupUi(this);

    connect( this, SIGNAL(accepted()),
             this, SLOT(onAccepted()) );
}

TabDialog::~TabDialog()
{
    delete ui;
}

void TabDialog::onAccepted()
{
    QString name = ui->lineEditTabName->text();
    if ( !name.isEmpty() )
        emit addTab(name);
}

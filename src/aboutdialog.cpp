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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // load about document from resources
    QFile f(":/aboutdialog.html");
    if ( f.open(QIODevice::ReadOnly) ) {
        ui->textEdit->setText( QString::fromUtf8(f.readAll()) );
    }

    connect(this, SIGNAL(finished(int)), SLOT(onFinished(int)));
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    /* try to resize the dialog so that vertical scrollbar in the about
     * document is hidden
     */
    restoreGeometry( ConfigurationManager::instance(this)->windowGeometry(
            objectName()) );
}

void AboutDialog::onFinished(int)
{
    ConfigurationManager::instance()->windowGeometry(
            objectName(), saveGeometry() );
}

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
#include "gui/logdialog.h"
#include "ui_logdialog.h"

#include "common/common.h"
#include "common/log.h"

#include <QRegExp>
#include <QTextCharFormat>
#include <QTextCursor>

LogDialog::LogDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LogDialog)
{
    ui->setupUi(this);

    QFont font("Monospace");
    ui->textBrowserLog->setFont(font);
    ui->textBrowserLog->setPlainText( readLogFile() );

    decorateLog();

    ui->textBrowserLog->moveCursor(QTextCursor::End);
}

LogDialog::~LogDialog()
{
    delete ui;
}

void LogDialog::decorateLog()
{
    QFont boldFont = font();
    boldFont.setBold(true);

    QTextCharFormat normalFormat;
    normalFormat.setFont(boldFont);
    normalFormat.setBackground(Qt::white);
    normalFormat.setForeground(Qt::black);

    QTextCharFormat errorLogLevelFormat = normalFormat;
    errorLogLevelFormat.setForeground(Qt::red);

    QTextCharFormat warningLogLevelFormat = normalFormat;
    warningLogLevelFormat.setForeground(Qt::darkRed);

    QTextCharFormat debugLogLevelFormat = normalFormat;
    debugLogLevelFormat.setForeground(QColor(100, 100, 200));

    QTextCharFormat traceLogLevelFormat = normalFormat;
    traceLogLevelFormat.setForeground(QColor(200, 150, 100));

    QTextDocument *doc = ui->textBrowserLog->document();

    const QString prefix = "CopyQ ";
    QRegExp rePrefix("^" + prefix + ".*: ");
    rePrefix.setMinimal(true);

    for ( QTextCursor tc = doc->find(rePrefix);
          !tc.isNull();
          tc = doc->find(rePrefix, tc) )
    {
        const QString text = tc.selectedText().mid( prefix.size() );
        const QTextCharFormat &format =
                    text.contains("ERROR") ? errorLogLevelFormat
                  : text.contains("warning") ? warningLogLevelFormat
                  : text.contains("DEBUG") ? debugLogLevelFormat
                  : text.contains("TRACE") ? traceLogLevelFormat
                  : normalFormat;
        tc.insertText(text, format);
    }

    QTextCharFormat stringFormat;
    stringFormat.setForeground(Qt::darkGreen);

    QRegExp reString("\"[^\"]*\"|'[^']*'");

    for ( QTextCursor tc = doc->find(reString);
          !tc.isNull();
          tc = doc->find(reString, tc) )
    {
        tc.setCharFormat(stringFormat);
    }
}

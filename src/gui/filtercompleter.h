/*
    Copyright (c) 2015, Lukas Holecek <hluk@email.cz>

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

#ifndef FILTERCOMPLETER_H
#define FILTERCOMPLETER_H

#include <QCompleter>

class QLineEdit;

class FilterCompleter : public QCompleter
{
    Q_OBJECT
public:
    static void installCompleter(QLineEdit *lineEdit);

private slots:
    void onTextEdited(const QString &text);
    void onEditingFinished();

private:
    explicit FilterCompleter(QLineEdit *lineEdit);

    QLineEdit *m_lineEdit;
    QString m_lastText;
};

#endif // FILTERCOMPLETER_H

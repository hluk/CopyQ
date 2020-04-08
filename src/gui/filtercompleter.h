/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include <QStringList>

class QLineEdit;

class FilterCompleter final : public QCompleter
{
    Q_OBJECT
    Q_PROPERTY(QStringList history READ history WRITE setHistory)
public:
    static void installCompleter(QLineEdit *lineEdit);
    static void removeCompleter(QLineEdit *lineEdit);

    QStringList history() const;
    void setHistory(const QStringList &history);

private:
    void onTextEdited(const QString &text);
    void onEditingFinished();
    void onComplete();

    explicit FilterCompleter(QLineEdit *lineEdit);
    void setUnfiltered(bool unfiltered);
    void prependItem(const QString &item);

    QLineEdit *m_lineEdit;
    QString m_lastText;
};

#endif // FILTERCOMPLETER_H

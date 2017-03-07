/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#ifndef COMMANDCOMPLETER_H
#define COMMANDCOMPLETER_H

#include <QObject>

class QCompleter;
class QPlainTextEdit;

class CommandCompleter : public QObject {
    Q_OBJECT
public:
    explicit CommandCompleter(QPlainTextEdit *editor);

    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateCompletion(bool forceShow = false);
    void insertCompletion(const QString &completion);
    void showCompletion();

private:
    QString textUnderCursor() const;

    QPlainTextEdit *m_editor;
    QCompleter *m_completer;
};

#endif // COMMANDCOMPLETER_H

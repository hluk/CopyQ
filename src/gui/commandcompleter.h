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

#ifndef COMMANDCOMPLETER_H
#define COMMANDCOMPLETER_H

#include <QObject>

class QCompleter;
class QPlainTextEdit;

class CommandCompleter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWidget* popup READ popup)

public:
    explicit CommandCompleter(QPlainTextEdit *editor);

    bool eventFilter(QObject *watched, QEvent *event) override;

    QWidget *popup() const;

private:
    void onTextChanged();
    void updateCompletion(bool forceShow);
    void insertCompletion(const QString &completion);
    void showCompletion();

    QString textUnderCursor() const;

    QPlainTextEdit *m_editor;
    QCompleter *m_completer;
};

#endif // COMMANDCOMPLETER_H

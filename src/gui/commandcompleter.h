// SPDX-License-Identifier: GPL-3.0-or-later

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

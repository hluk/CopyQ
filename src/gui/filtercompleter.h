// SPDX-License-Identifier: GPL-3.0-or-later

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

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

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

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

#include "filtercompleter.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QAction>
#include <QLineEdit>
#include <QMoveEvent>

namespace {

const int maxCompletionItems = 100;

class CompletionModel final : public QAbstractListModel
{
public:
    explicit CompletionModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_items.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (index.isValid() && (role == Qt::EditRole || role == Qt::DisplayRole))
            return m_items[index.row()];

        return QVariant();
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if (!index.isValid() && role == Qt::EditRole) {
            const QString text = value.toString();
            removeAll(text);
            crop(maxCompletionItems - 1);
            prepend(text);
        }

        return false;
    }

    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override
    {
        const auto end = row + count;
        if ( parent.isValid() || row < 0 || end > rowCount() )
            return false;

        beginRemoveRows(QModelIndex(), row, end);
        m_items.erase( m_items.begin() + row, m_items.begin() + end );
        endRemoveRows();

        return true;
    }

private:
    void prepend(const QString &text)
    {
        beginInsertRows(QModelIndex(), 0, 0);
        m_items.prepend(text);
        endInsertRows();
    }

    void removeAll(const QString &text)
    {
        for ( int row = m_items.indexOf(text);
              row != -1;
              row = m_items.indexOf(text, row) )
        {
            removeRows(row, 1);
        }
    }

    void crop(int maxItems)
    {
        const int itemCount = m_items.size();
        if (itemCount > maxItems)
            removeRows(maxItems, itemCount - maxItems);
    }

    QStringList m_items;
};

} // namespace

void FilterCompleter::installCompleter(QLineEdit *lineEdit)
{
    Q_ASSERT(lineEdit);
    new FilterCompleter(lineEdit);
}

void FilterCompleter::removeCompleter(QLineEdit *lineEdit)
{
    lineEdit->setCompleter(nullptr);
}

QStringList FilterCompleter::history() const
{
    QStringList history;

    for (int i = 0; i < model()->rowCount(); ++i) {
        const QModelIndex index = model()->index(i, 0);
        history.append( index.data(Qt::EditRole).toString() );
    }

    return history;
}

void FilterCompleter::setHistory(const QStringList &history)
{
    model()->removeRows( 0, model()->rowCount() );
    for (int i = history.size() - 1; i >= 0; --i)
        prependItem(history[i]);
}

void FilterCompleter::onTextEdited(const QString &text)
{
    m_lastText = text;
    setUnfiltered(false);
}

void FilterCompleter::onEditingFinished()
{
    prependItem(m_lastText);
    m_lastText.clear();

    setUnfiltered(false);
}

void FilterCompleter::onComplete()
{
    if (m_lineEdit->text().isEmpty()) {
        setUnfiltered(true);
        const QModelIndex firstIndex = model()->index(0, 0);
        const QString text = model()->data(firstIndex, Qt::EditRole).toString();
        m_lineEdit->setText(text);
    } else {
        complete();
    }
}

FilterCompleter::FilterCompleter(QLineEdit *lineEdit)
    : QCompleter(lineEdit)
    , m_lineEdit(lineEdit)
{
    setModel(new CompletionModel(this));
    setWrapAround(true);
    setUnfiltered(false);

    QWidget *window = lineEdit->window();
    if (window) {
        auto act = new QAction(this);
        act->setShortcut(tr("Alt+Down", "Filter completion shortcut"));
        connect(act, &QAction::triggered, this, &FilterCompleter::onComplete);
        window->addAction(act);
    }

    // Postpone prepending item to completion list because incorrect
    // item will be completed otherwise (prepending shifts rows).
    connect( lineEdit, &QLineEdit::editingFinished,
             this, &FilterCompleter::onEditingFinished, Qt::QueuedConnection );
    connect( lineEdit, &QLineEdit::textEdited,
             this, &FilterCompleter::onTextEdited );

    lineEdit->setCompleter(this);
}

void FilterCompleter::setUnfiltered(bool unfiltered)
{
    setCompletionMode(unfiltered ? QCompleter::UnfilteredPopupCompletion
                                 : QCompleter::PopupCompletion);
}

void FilterCompleter::prependItem(const QString &item)
{
    if ( !item.isEmpty() )
        model()->setData(QModelIndex(), item);
}

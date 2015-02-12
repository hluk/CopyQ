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

#include "filtercompleter.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QAction>
#include <QLineEdit>
#include <QMoveEvent>
#include <QPointer>

namespace {

const int maxCompletionItems = 100;

class CompletionModel : public QAbstractListModel
{
public:
    explicit CompletionModel(QObject *parent)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent) const
    {
        return parent.isValid() ? 0 : m_items.size();
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (index.isValid() && (role == Qt::EditRole || role == Qt::DisplayRole))
            return m_items[index.row()];

        return QVariant();
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role)
    {
        if (!index.isValid() && role == Qt::EditRole) {
            const QString text = value.toString();
            removeAll(text);
            crop(maxCompletionItems - 1);
            prepend(text);
        }

        return false;
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
            beginRemoveRows(QModelIndex(), row, row);
            m_items.removeAt(row);
            endRemoveRows();
        }
    }

    void crop(int maxItems)
    {
        const int itemCount = m_items.size();
        if (itemCount >= maxItems) {
            beginRemoveRows(QModelIndex(), maxItems, itemCount);
            m_items.erase(m_items.begin() + maxItems);
            endRemoveRows();
        }
    }

    QStringList m_items;
};

} // namespace

void FilterCompleter::installCompleter(QLineEdit *lineEdit)
{
    Q_ASSERT(lineEdit);
    new FilterCompleter(lineEdit);
}

void FilterCompleter::onTextEdited(const QString &text)
{
    m_lastText = text;
    setUnfiltered(false);
}

void FilterCompleter::onEditingFinished()
{
    if (!m_lastText.isEmpty()) {
        model()->setData(QModelIndex(), m_lastText);
        m_lastText.clear();
    }

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
        QAction *act = new QAction(this);
        act->setShortcut(tr("Alt+Down", "Filter completion shortcut"));
        connect(act, SIGNAL(triggered()), this, SLOT(onComplete()));
        window->addAction(act);
    }

    // Postpone prepending item to completion list because incorrect
    // item will be completed otherwise (prepending shifts rows).
    connect( lineEdit, SIGNAL(editingFinished()),
             this, SLOT(onEditingFinished()), Qt::QueuedConnection );
    connect( lineEdit, SIGNAL(textEdited(QString)),
             this, SLOT(onTextEdited(QString)) );

    lineEdit->setCompleter(this);
}

void FilterCompleter::setUnfiltered(bool unfiltered)
{
    setCompletionMode(unfiltered ? QCompleter::UnfilteredPopupCompletion
                                 : QCompleter::PopupCompletion);
}

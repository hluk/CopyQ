/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "itemwidget.h"

#include "common/contenttype.h"
#include "item/itemeditor.h"

#include <QAbstractItemModel>
#include <QFont>
#include <QModelIndex>
#include <QPalette>
#include <QTextEdit>
#include <QWidget>

ItemWidget::ItemWidget(QWidget *widget)
    : m_re()
    , m_widget(widget)
{
    Q_ASSERT(widget != NULL);

    // Object name for style sheet.
    widget->setObjectName("item");

    // Item widgets are not focusable.
    widget->setFocusPolicy(Qt::NoFocus);

    // Limit size of items.
    widget->setMaximumSize(2048, 2048);

    // Disable drag'n'drop by default.
    widget->setAcceptDrops(false);
}

void ItemWidget::setHighlight(const QRegExp &re, const QFont &highlightFont,
                              const QPalette &highlightPalette)
{
    QPalette palette( widget()->palette() );
    palette.setColor(QPalette::Highlight, highlightPalette.base().color());
    palette.setColor(QPalette::HighlightedText, highlightPalette.text().color());
    widget()->setPalette(palette);

    if (m_re == re)
        return;
    m_re = re;
    highlight(re, highlightFont, highlightPalette);
}

QWidget *ItemWidget::createEditor(QWidget *parent) const
{
    QTextEdit *editor = new QTextEdit(parent);
    editor->setFrameShape(QFrame::NoFrame);
    return editor;
}

void ItemWidget::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QTextEdit *textEdit = qobject_cast<QTextEdit *>(editor);
    if (textEdit != NULL) {
        const QString text = index.data(Qt::EditRole).toString();
        textEdit->setPlainText(text);
        textEdit->selectAll();
    }
}

void ItemWidget::setModelData(QWidget *editor, QAbstractItemModel *model,
                              const QModelIndex &index) const
{
    QTextEdit *textEdit = qobject_cast<QTextEdit*>(editor);
    if (textEdit != NULL) {
        model->setData(index, textEdit->toPlainText());
        textEdit->document()->setModified(false);
    }
}

bool ItemWidget::hasChanges(QWidget *editor) const
{
    QTextEdit *textEdit = (qobject_cast<QTextEdit *>(editor));
    return textEdit != NULL && textEdit->document() && textEdit->document()->isModified();
}

QObject *ItemWidget::createExternalEditor(const QModelIndex &, QWidget *) const
{
    return NULL;
}

void ItemWidget::updateSize(const QSize &maximumSize)
{
    QWidget *w = widget();
    w->setMaximumSize(maximumSize);
    const int h = w->heightForWidth(maximumSize.width());
    if (h > 0)
        w->setFixedSize( maximumSize.width(), h );
    else
        w->resize(w->sizeHint());
}

void ItemWidget::setCurrent(bool)
{
}


ItemWidget *ItemLoaderInterface::create(const QModelIndex &, QWidget *) const
{
    return NULL;
}

bool ItemLoaderInterface::canLoadItems(QFile *)
{
    return false;
}

bool ItemLoaderInterface::canSaveItems(const QAbstractItemModel &)
{
    return false;
}

bool ItemLoaderInterface::loadItems(QAbstractItemModel *, QFile *)
{
    return false;
}

bool ItemLoaderInterface::saveItems(const QAbstractItemModel &, QFile *)
{
    return false;
}

bool ItemLoaderInterface::initializeTab(QAbstractItemModel *)
{
    return false;
}

void ItemLoaderInterface::uninitializeTab(QAbstractItemModel *)
{
}

ItemWidget *ItemLoaderInterface::transform(ItemWidget *, const QModelIndex &)
{
    return NULL;
}

bool ItemLoaderInterface::canRemoveItems(const QList<QModelIndex> &)
{
    return true;
}

bool ItemLoaderInterface::canMoveItems(const QList<QModelIndex> &)
{
    return true;
}

void ItemLoaderInterface::itemsRemovedByUser(const QList<QModelIndex> &)
{
}

QVariantMap ItemLoaderInterface::copyItem(const QAbstractItemModel &, const QVariantMap &itemData)
{
    return itemData;
}

bool ItemLoaderInterface::matches(const QModelIndex &, const QRegExp &) const
{
    return false;
}

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

#ifndef ITEMEDITORWIDGET_H
#define ITEMEDITORWIDGET_H

#include "item/itemwidget.h"

#include <QPersistentModelIndex>
#include <QRegularExpression>
#include <QTextEdit>

#include <memory>

class ItemWidget;
class QAbstractItemModel;
class QPlainTextEdit;
class QTextCursor;
class QTextDocument;
class QWidget;

/**
 * Internal editor widget for items.
 */
class ItemEditorWidget final : public QTextEdit
{
    Q_OBJECT
public:
    ItemEditorWidget(const QModelIndex &index, bool editNotes, QWidget *parent = nullptr);

    bool isValid() const;

    bool hasChanges() const;

    void setHasChanges(bool hasChanges);

    void setSaveOnReturnKey(bool enabled);

    QVariantMap data() const;

    QModelIndex index() const { return m_index; }

    void search(const QRegularExpression &re);

    void findNext();

    void findPrevious();

    QWidget *createToolbar(QWidget *parent);

signals:
    void save();
    void cancel();
    void invalidate();
    void searchRequest();

protected:
    void keyPressEvent(QKeyEvent *event) override;

    bool canInsertFromMimeData(const QMimeData *source) const override;

    void insertFromMimeData(const QMimeData *source) override;

private:
    void saveAndExit();

    void changeSelectionFont();
    void toggleBoldText();
    void toggleItalicText();
    void toggleUnderlineText();
    void toggleStrikethroughText();
    void setForeground();
    void setBackground();
    void eraseStyle();

    void search(bool backwards);

    QPersistentModelIndex m_index;
    bool m_saveOnReturnKey;
    bool m_editNotes;
    QRegularExpression m_re;
};

#endif // ITEMEDITORWIDGET_H

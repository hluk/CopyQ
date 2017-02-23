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

#ifndef ITEMEDITORWIDGET_H
#define ITEMEDITORWIDGET_H

#include <QPersistentModelIndex>
#include <QRegExp>
#include <QWidget>

class ItemWidget;
class QAbstractItemModel;
class QPlainTextEdit;
class QTextCursor;
class QTextDocument;
class QToolBar;

/**
 * Internal editor widget for items.
 */
class ItemEditorWidget : public QWidget
{
    Q_OBJECT
public:
    ItemEditorWidget(ItemWidget *itemWidget, const QModelIndex &index, bool editNotes,
                     QWidget *parent = nullptr);

    bool isValid() const;

    void commitData(QAbstractItemModel *model) const;

    bool hasChanges() const;

    void setEditorPalette(const QPalette &palette);

    void setEditorFont(const QFont &font);

    void setSaveOnReturnKey(bool enabled);

    QModelIndex index() const { return m_index; }

    void search(const QRegExp &re);

    void findNext(const QRegExp &re);

    void findPrevious(const QRegExp &re);

signals:
    void save();
    void cancel();
    void invalidate();
    void searchRequest();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void onItemWidgetDestroyed();
    void saveAndExit();

    void setFont();
    void toggleBoldText();
    void toggleItalicText();
    void toggleUnderlineText();
    void toggleStrikethroughText();
    void setForeground();
    void setBackground();
    void eraseStyle();

private:
    QWidget *createEditor(const ItemWidget *itemWidget);
    void initEditor(QWidget *editor);
    void initMenuItems();

    void search(const QRegExp &re, bool backwards);

    template <typename TextEdit>
    TextEdit *editor() const;

    QTextDocument *document() const;
    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &tc);

    ItemWidget *m_itemWidget;
    QPersistentModelIndex m_index;
    QWidget *m_editor;
    QPlainTextEdit *m_noteEditor;
    QToolBar *m_toolBar;
    bool m_saveOnReturnKey;
};

#endif // ITEMEDITORWIDGET_H

/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "item/itemeditorwidget.h"

#include "common/contenttype.h"
#include "item/itemwidget.h"

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QIcon>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

const QIcon iconSave(const QColor &color = QColor()) { return getIcon("document-save", IconSave, color); }
const QIcon iconCancel(const QColor &color = QColor()) { return getIcon("document-revert", IconRemove, color); }
const QIcon iconUndo(const QColor &color = QColor()) { return getIcon("edit-undo", IconUndo, color); }
const QIcon iconRedo(const QColor &color = QColor()) { return getIcon("edit-redo", IconRepeat, color); }

} // namespace

ItemEditorWidget::ItemEditorWidget(const ItemWidget *itemWidget, const QModelIndex &index,
                                   bool editNotes, const QFont &font, const QPalette &palette,
                                   bool saveOnReturnKey, QWidget *parent)
    : QWidget(parent)
    , m_itemWidget(itemWidget)
    , m_index(index)
    , m_editor(NULL)
    , m_noteEditor(NULL)
    , m_saveOnReturnKey(saveOnReturnKey)
{
    m_noteEditor = editNotes ? new QPlainTextEdit(this) : NULL;
    QWidget *editor = editNotes ? m_noteEditor : itemWidget->createEditor(this);

    if (editor == NULL) {
        m_itemWidget = NULL;
    } else {
        connect( m_itemWidget->widget(), SIGNAL(destroyed()),
                 this, SLOT(onItemWidgetDestroyed()) );
        initEditor(editor, font, palette);
        if (m_noteEditor != NULL)
            m_noteEditor->setPlainText( index.data(contentType::notes).toString() );
        else
            itemWidget->setEditorData(editor, index);
    }
}

void ItemEditorWidget::commitData(QAbstractItemModel *model) const
{
    if ( hasChanges() ) {
        if (m_noteEditor != NULL)
            model->setData(m_index, m_noteEditor->toPlainText(), contentType::notes);
        else
            m_itemWidget->setModelData(m_editor, model, m_index);
    }
}

bool ItemEditorWidget::hasChanges() const
{
    return isValid() && m_itemWidget->hasChanges(m_editor);
}

bool ItemEditorWidget::eventFilter(QObject *object, QEvent *event)
{
    if ( object == m_editor && event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
        int k = keyevent->key();

        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            Qt::KeyboardModifiers mods = keyevent->modifiers();
            if ( (mods & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == 0 ) {
                if (m_saveOnReturnKey) {
                    if (mods.testFlag(Qt::ControlModifier) ) {
                        keyevent->setModifiers(mods & ~Qt::ControlModifier);
                        return false;
                    }

                    emit save();
                    return true;
                } else if ( mods.testFlag(Qt::ControlModifier) ) {
                    emit save();
                    return true;
                }
            }
        }
    }

    return false;
}

void ItemEditorWidget::onItemWidgetDestroyed()
{
    m_itemWidget = NULL;
    emit invalidate();
}

void ItemEditorWidget::initEditor(QWidget *editor, const QFont &font, const QPalette &palette)
{
    m_editor = editor;

    setFocusPolicy(Qt::StrongFocus);

    setPalette(palette);
    editor->setPalette(palette);
    editor->setFont(font);
    editor->installEventFilter(this);

    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setBackgroundRole(QPalette::Base);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->addWidget(toolBar);
    layout->addWidget(editor);

    setFocusProxy(editor);

    const QColor color = getDefaultIconColor( palette.color(QPalette::Base) );

    QAction *act;
    act = new QAction( iconSave(color), tr("Save"), editor );
    toolBar->addAction(act);
    act->setToolTip( tr("Save Item (<strong>F2</strong>)") );
    act->setShortcut( QKeySequence(tr("F2", "Shortcut to save item editor changes")) );
    connect( act, SIGNAL(triggered()),
             this, SIGNAL(save()) );

    act = new QAction( iconCancel(color), tr("Cancel"), editor );
    toolBar->addAction(act);
    act->setToolTip( tr("Cancel Editing and Revert Changes") );
    act->setShortcut( QKeySequence(tr("Escape", "Shortcut to revert item editor changes")) );
    connect( act, SIGNAL(triggered()),
             this, SIGNAL(cancel()) );

    QPlainTextEdit *plainTextEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (plainTextEdit != NULL) {
        plainTextEdit->setFrameShape(QFrame::NoFrame);

        act = new QAction( iconUndo(color), tr("Undo"), editor );
        toolBar->addAction(act);
        act->setShortcut(QKeySequence::Undo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(undo()) );
        connect( plainTextEdit, SIGNAL(undoAvailable(bool)), act, SLOT(setEnabled(bool)) );

        act = new QAction( iconRedo(color), tr("Redo"), editor );
        toolBar->addAction(act);
        act->setShortcut(QKeySequence::Redo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(redo()) );
        connect( plainTextEdit, SIGNAL(redoAvailable(bool)), act, SLOT(setEnabled(bool)) );
    }
}

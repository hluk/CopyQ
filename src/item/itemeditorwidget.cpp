/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QIcon>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconCancel() { return getIcon("document-revert", IconRemove); }
const QIcon iconUndo() { return getIcon("edit-undo", IconUndo); }
const QIcon iconRedo() { return getIcon("edit-redo", IconRepeat); }

} // namespace

ItemEditorWidget::ItemEditorWidget(ItemWidget *itemWidget, const QModelIndex &index,
                                   bool editNotes, QWidget *parent)
    : QWidget(parent)
    , m_itemWidget(itemWidget)
    , m_index(index)
    , m_editor(NULL)
    , m_noteEditor(NULL)
    , m_toolBar(NULL)
    , m_saveOnReturnKey(false)
{
    m_noteEditor = editNotes ? new QPlainTextEdit(parent) : NULL;
    QWidget *editor = editNotes ? m_noteEditor : createEditor(itemWidget);

    if (editor == NULL) {
        m_itemWidget = NULL;
    } else {
        connect( m_itemWidget->widget(), SIGNAL(destroyed()),
                 this, SLOT(onItemWidgetDestroyed()) );
        initEditor(editor);
        if (m_noteEditor != NULL)
            m_noteEditor->setPlainText( index.data(contentType::notes).toString() );
        else
            itemWidget->setEditorData(editor, index);
    }
}

bool ItemEditorWidget::isValid() const
{
    if ( !m_index.isValid() )
        return false;
    return (m_itemWidget != NULL && m_editor != NULL) || m_noteEditor != NULL;
}

void ItemEditorWidget::commitData(QAbstractItemModel *model) const
{
    if ( hasChanges() ) {
        if (m_noteEditor != NULL) {
            model->setData(m_index, m_noteEditor->toPlainText(), contentType::notes);
            m_noteEditor->document()->setModified(false);
        } else {
            m_itemWidget->setModelData(m_editor, model, m_index);
        }
    }
}

bool ItemEditorWidget::hasChanges() const
{
    if ( !m_index.isValid() )
        return false;
    if (m_noteEditor != NULL)
        return m_noteEditor->document()->isModified();
    return m_itemWidget != NULL && m_itemWidget->hasChanges(m_editor);
}

void ItemEditorWidget::setEditorPalette(const QPalette &palette)
{
    setPalette(palette);
    if (m_editor) {
        QPalette pal2 = palette;
        pal2.setColor(QPalette::Base, Qt::transparent);
        m_editor->setPalette(pal2);
        initMenuItems();
    }
}

void ItemEditorWidget::setEditorFont(const QFont &font)
{
    if (m_editor)
        m_editor->setFont(font);
}

void ItemEditorWidget::setSaveOnReturnKey(bool enabled)
{
    m_saveOnReturnKey = enabled;
}

bool ItemEditorWidget::eventFilter(QObject *object, QEvent *event)
{
    if ( object == m_editor && event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
        int k = keyevent->key();

        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            Qt::KeyboardModifiers mods = keyevent->modifiers();
            if ( (mods & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == 0 ) {
                bool controlPressed = mods.testFlag(Qt::ControlModifier);
                if (m_saveOnReturnKey && controlPressed ) {
                    keyevent->setModifiers(mods & ~Qt::ControlModifier);
                    return false;
                }
                if ( m_saveOnReturnKey || controlPressed ) {
                    saveAndExit();
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

void ItemEditorWidget::saveAndExit()
{
    emit save();
    emit invalidate();
}

QWidget *ItemEditorWidget::createEditor(const ItemWidget *itemWidget)
{
    QWidget *editor = itemWidget->createEditor(this);

    if (editor) {
        const QMetaObject *metaObject = editor->metaObject();
        if ( metaObject->indexOfSignal("save()") != -1 )
            connect( editor, SIGNAL(save()), SIGNAL(save()) );
        if ( metaObject->indexOfSignal("cancel()") != -1 )
            connect( editor, SIGNAL(cancel()), SIGNAL(cancel()) );
        if ( metaObject->indexOfSignal("invalidate()") != -1 )
            connect( editor, SIGNAL(invalidate()), SIGNAL(invalidate()) );
    }

    return editor;
}

void ItemEditorWidget::initEditor(QWidget *editor)
{
    m_editor = editor;

    setFocusPolicy(Qt::StrongFocus);

    editor->installEventFilter(this);

    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    m_toolBar = new QToolBar(this);
    m_toolBar->setBackgroundRole(QPalette::Base);
    m_toolBar->setStyleSheet("QToolBar{border:0;background:transparent}"
                             "QToolButton{background:transparent}");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->addWidget(m_toolBar);
    layout->addWidget(editor);

    setFocusProxy(editor);
}

void ItemEditorWidget::initMenuItems()
{
    Q_ASSERT(m_editor);

    foreach (QAction *action, m_toolBar->actions())
        delete action;

    QAction *act;
    act = new QAction( iconSave(), tr("Save"), m_editor );
    m_toolBar->addAction(act);
    act->setToolTip( tr("Save Item (<strong>F2</strong>)") );
    act->setShortcut( QKeySequence(tr("F2", "Shortcut to save item editor changes")) );
    connect( act, SIGNAL(triggered()),
             this, SLOT(saveAndExit()) );

    act = new QAction( iconCancel(), tr("Cancel"), m_editor );
    m_toolBar->addAction(act);
    act->setToolTip( tr("Cancel Editing and Revert Changes") );
    act->setShortcut( QKeySequence(tr("Escape", "Shortcut to revert item editor changes")) );
    connect( act, SIGNAL(triggered()),
             this, SIGNAL(cancel()) );

    QPlainTextEdit *plainTextEdit = qobject_cast<QPlainTextEdit*>(m_editor);
    if (plainTextEdit != NULL) {
        plainTextEdit->setFrameShape(QFrame::NoFrame);

        act = new QAction( iconUndo(), tr("Undo"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut(QKeySequence::Undo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(undo()) );
        connect( plainTextEdit, SIGNAL(undoAvailable(bool)), act, SLOT(setEnabled(bool)) );

        act = new QAction( iconRedo(), tr("Redo"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut(QKeySequence::Redo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), plainTextEdit, SLOT(redo()) );
        connect( plainTextEdit, SIGNAL(redoAvailable(bool)), act, SLOT(setEnabled(bool)) );
    }
}

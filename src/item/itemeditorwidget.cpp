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

#include "item/itemeditorwidget.h"

#include "common/contenttype.h"
#include "item/itemwidget.h"

#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QColorDialog>
#include <QFontDialog>
#include <QIcon>
#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconCancel() { return getIcon("document-revert", IconRemove); }

const QIcon iconUndo() { return getIcon("edit-undo", IconUndo); }
const QIcon iconRedo() { return getIcon("edit-redo", IconRepeat); }

const QIcon iconFont() { return getIcon("preferences-desktop-font", IconFont); }
const QIcon iconBold() { return getIcon("format-text-bold", IconBold); }
const QIcon iconItalic() { return getIcon("format-text-italic", IconItalic); }
const QIcon iconUnderline() { return getIcon("format-text-underline", IconUnderline); }
const QIcon iconStrikethrough() { return getIcon("format-text-strikethrough", IconStrikethrough); }

const QIcon iconForeground() { return getIcon(IconSquareO); }
const QIcon iconBackground() { return getIcon(IconSquare); }

const QIcon iconEraseStyle() { return getIcon(IconEraser); }

const QIcon iconSearch() { return getIcon("edit-find", IconSearch); }

} // namespace

ItemEditorWidget::ItemEditorWidget(ItemWidget *itemWidget, const QModelIndex &index,
                                   bool editNotes, QWidget *parent)
    : QWidget(parent)
    , m_itemWidget(itemWidget)
    , m_index(index)
    , m_editor(nullptr)
    , m_noteEditor(nullptr)
    , m_toolBar(nullptr)
    , m_saveOnReturnKey(false)
{
    m_noteEditor = editNotes ? new QPlainTextEdit(parent) : nullptr;
    QWidget *editor = editNotes ? m_noteEditor : createEditor(itemWidget);

    if (editor == nullptr) {
        m_itemWidget = nullptr;
    } else {
        connect( m_itemWidget->widget(), SIGNAL(destroyed()),
                 this, SLOT(onItemWidgetDestroyed()) );
        initEditor(editor);
        if (m_noteEditor != nullptr)
            m_noteEditor->setPlainText( index.data(contentType::notes).toString() );
        else
            itemWidget->setEditorData(editor, index);
    }
}

bool ItemEditorWidget::isValid() const
{
    if ( !m_index.isValid() )
        return false;
    return (m_itemWidget != nullptr && m_editor != nullptr) || m_noteEditor != nullptr;
}

void ItemEditorWidget::commitData(QAbstractItemModel *model) const
{
    if ( hasChanges() ) {
        if (m_noteEditor != nullptr) {
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
    if (m_noteEditor != nullptr)
        return m_noteEditor->document()->isModified();
    return m_itemWidget != nullptr && m_itemWidget->hasChanges(m_editor);
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

void ItemEditorWidget::search(const QRegExp &re)
{
    if ( !re.isValid() || re.isEmpty() )
        return;

    auto tc = textCursor();
    tc.setPosition(tc.selectionStart());
    setTextCursor(tc);
    findNext(re);
}

void ItemEditorWidget::findNext(const QRegExp &re)
{
    search(re, false);
}

void ItemEditorWidget::findPrevious(const QRegExp &re)
{
    search(re, true);
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
    m_itemWidget = nullptr;
    emit invalidate();
}

void ItemEditorWidget::saveAndExit()
{
    emit save();
    emit invalidate();
}

void ItemEditorWidget::setFont()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();

    QFontDialog dialog(this);
    dialog.setCurrentFont( format.font() );

    if ( dialog.exec() == QDialog::Accepted ) {
        const QFont font = dialog.selectedFont();
        format.setFont(font);
        tc.setCharFormat(format);
    }
}

void ItemEditorWidget::toggleBoldText()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();
    const int weight = format.fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold;
    format.setFontWeight(weight);
    tc.setCharFormat(format);
}

void ItemEditorWidget::toggleItalicText()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();
    format.setFontItalic( !format.fontItalic() );
    tc.setCharFormat(format);
}

void ItemEditorWidget::toggleUnderlineText()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();
    format.setFontUnderline( !format.fontUnderline() );
    tc.setCharFormat(format);
}

void ItemEditorWidget::toggleStrikethroughText()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();
    format.setFontStrikeOut( !format.fontStrikeOut() );
    tc.setCharFormat(format);
}

void ItemEditorWidget::setForeground()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();

    QColorDialog dialog(this);
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor( format.foreground().color() );

    if ( dialog.exec() == QDialog::Accepted ) {
        const QColor color = dialog.selectedColor();
        format.setForeground(color);
        tc.setCharFormat(format);
    }
}

void ItemEditorWidget::setBackground()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();

    QColorDialog dialog(this);
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor( format.background().color() );

    if ( dialog.exec() == QDialog::Accepted ) {
        const QColor color = dialog.selectedColor();
        format.setBackground(color);
        tc.setCharFormat(format);
    }
}

void ItemEditorWidget::eraseStyle()
{
    textCursor().setCharFormat( QTextCharFormat() );
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

    auto layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->addWidget(m_toolBar);
    layout->addWidget(editor);

    setFocusProxy(editor);
}

void ItemEditorWidget::initMenuItems()
{
    Q_ASSERT(m_editor);

    auto frame = qobject_cast<QFrame*>(m_editor);
    if (frame)
        frame->setFrameShape(QFrame::NoFrame);

    for (auto action : m_toolBar->actions())
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

    auto doc = document();

    if (document()) {
        m_toolBar->addSeparator();

        act = new QAction( iconUndo(), tr("Undo"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut(QKeySequence::Undo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), doc, SLOT(undo()) );
        connect( doc, SIGNAL(undoAvailable(bool)), act, SLOT(setEnabled(bool)) );

        act = new QAction( iconRedo(), tr("Redo"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut(QKeySequence::Redo);
        act->setEnabled(false);
        connect( act, SIGNAL(triggered()), doc, SLOT(redo()) );
        connect( doc, SIGNAL(redoAvailable(bool)), act, SLOT(setEnabled(bool)) );

        m_toolBar->addSeparator();

        act = new QAction( iconFont(), tr("Font"), m_editor );
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SLOT(setFont()) );

        act = new QAction( iconBold(), tr("Bold"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut( QKeySequence::Bold );
        connect( act, SIGNAL(triggered()),
                 this, SLOT(toggleBoldText()) );

        act = new QAction( iconItalic(), tr("Italic"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut( QKeySequence::Italic );
        connect( act, SIGNAL(triggered()),
                 this, SLOT(toggleItalicText()) );

        act = new QAction( iconUnderline(), tr("Underline"), m_editor );
        m_toolBar->addAction(act);
        act->setShortcut( QKeySequence::Underline );
        connect( act, SIGNAL(triggered()),
                 this, SLOT(toggleUnderlineText()) );

        act = new QAction( iconStrikethrough(), tr("Strikethrough"), m_editor );
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SLOT(toggleStrikethroughText()) );

        m_toolBar->addSeparator();

        act = new QAction( iconForeground(), tr("Foreground"), m_editor );
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SLOT(setForeground()) );

        act = new QAction( iconBackground(), tr("Background"), m_editor );
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SLOT(setBackground()) );

        m_toolBar->addSeparator();

        act = new QAction( iconEraseStyle(), tr("Erase Style"), m_editor );
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SLOT(eraseStyle()) );

        m_toolBar->addSeparator();

        act = new QAction( iconSearch(), tr("Search"), m_editor );
        act->setShortcuts(QKeySequence::Find);
        m_toolBar->addAction(act);
        connect( act, SIGNAL(triggered()),
                 this, SIGNAL(searchRequest()) );
    }
}

void ItemEditorWidget::search(const QRegExp &re, bool backwards)
{
    if ( !re.isValid() )
        return;

    auto tc = textCursor();
    if ( tc.isNull() )
        return;

    QTextDocument::FindFlags flags;
    if (backwards)
        flags = QTextDocument::FindBackward;

    auto tc2 = tc.document()->find(re, tc, flags);
    if (tc2.isNull()) {
        tc2 = tc;
        if (backwards)
            tc2.movePosition(QTextCursor::End);
        else
            tc2.movePosition(QTextCursor::Start);
        tc2 = tc.document()->find(re, tc2, flags);
    }

    if (!tc2.isNull())
        setTextCursor(tc2);
}

template<typename TextEdit>
TextEdit *ItemEditorWidget::editor() const
{
    auto textEdit = qobject_cast<TextEdit*>(m_editor);
    return textEdit ? textEdit : m_editor->findChild<TextEdit*>();
}

QTextDocument *ItemEditorWidget::document() const
{
    auto plainTextEdit = editor<QPlainTextEdit>();
    if (plainTextEdit)
        return plainTextEdit->document();

    auto textEdit = editor<QTextEdit>();
    if (textEdit)
        return textEdit->document();

    return nullptr;
}

QTextCursor ItemEditorWidget::textCursor() const
{
    auto plainTextEdit = editor<QPlainTextEdit>();
    if (plainTextEdit)
        return plainTextEdit->textCursor();

    auto textEdit = editor<QTextEdit>();
    if (textEdit)
        return textEdit->textCursor();

    return QTextCursor();
}

void ItemEditorWidget::setTextCursor(const QTextCursor &tc)
{
    auto plainTextEdit = editor<QPlainTextEdit>();
    if (plainTextEdit) {
        plainTextEdit->setTextCursor(tc);
    } else {
        auto textEdit = editor<QTextEdit>();
        if (textEdit)
            textEdit->setTextCursor(tc);
    }
}

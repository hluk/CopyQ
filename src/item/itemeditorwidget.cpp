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

#include "item/itemeditorwidget.h"

#include "item/itemwidget.h"

#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QColorDialog>
#include <QFontDialog>
#include <QIcon>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QToolBar>

namespace {

const QIcon iconSave() { return getIcon("document-save", IconSave); }
const QIcon iconCancel() { return getIcon("document-revert", IconTrash); }

const QIcon iconUndo() { return getIcon("edit-undo", IconUndo); }
const QIcon iconRedo() { return getIcon("edit-redo", IconRedo); }

const QIcon iconFont() { return getIcon("preferences-desktop-font", IconFont); }
const QIcon iconBold() { return getIcon("format-text-bold", IconBold); }
const QIcon iconItalic() { return getIcon("format-text-italic", IconItalic); }
const QIcon iconUnderline() { return getIcon("format-text-underline", IconUnderline); }
const QIcon iconStrikethrough() { return getIcon("format-text-strikethrough", IconStrikethrough); }

const QIcon iconForeground() { return getIcon(IconPaintBrush); }
const QIcon iconBackground() { return getIcon(IconSquare); }

const QIcon iconEraseStyle() { return getIcon(IconEraser); }

const QIcon iconSearch() { return getIcon("edit-find", IconSearch); }

bool containsRichText(const QTextDocument &document)
{
    return document.allFormats().size() > 3;
}

QString findImageFormat(const QMimeData &data)
{
    const auto imageFormats = {"image/svg+xml", "image/png", "image/bmp", "image/jpeg", "image/gif"};
    for (const auto &format : imageFormats) {
        if ( data.hasFormat(format) )
            return format;
    }

    return QString();
}

} // namespace

ItemEditorWidget::ItemEditorWidget(const QModelIndex &index, bool editNotes, QWidget *parent)
    : QTextEdit(parent)
    , m_index(index)
    , m_saveOnReturnKey(false)
    , m_editNotes(editNotes)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
}

bool ItemEditorWidget::isValid() const
{
    return m_index.isValid();
}

bool ItemEditorWidget::hasChanges() const
{
    return isValid() && document()->isModified();
}

void ItemEditorWidget::setHasChanges(bool hasChanges)
{
    document()->setModified(hasChanges);
}

void ItemEditorWidget::setSaveOnReturnKey(bool enabled)
{
    m_saveOnReturnKey = enabled;
}

QVariantMap ItemEditorWidget::data() const
{
    QVariantMap data;
    if (m_editNotes) {
        setTextData( &data, toPlainText(), mimeItemNotes );
    } else {
        setTextData( &data, toPlainText(), mimeText );
        if ( containsRichText(*document()) )
            setTextData( &data, toHtml(), mimeHtml );
    }
    return data;
}

void ItemEditorWidget::search(const QRegularExpression &re)
{
    if ( !re.isValid() || re.pattern().isEmpty() )
        return;

    auto tc = textCursor();
    tc.setPosition(tc.selectionStart());
    setTextCursor(tc);
    m_re = re;
    findNext();
}

void ItemEditorWidget::findNext()
{
    search(false);
}

void ItemEditorWidget::findPrevious()
{
    search(true);
}

void ItemEditorWidget::keyPressEvent(QKeyEvent *event)
{
    QKeyEvent *keyevent = static_cast<QKeyEvent *>(event);
    int k = keyevent->key();

    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        Qt::KeyboardModifiers mods = keyevent->modifiers();
        if ( (mods & (Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == 0 ) {
            bool controlPressed = mods.testFlag(Qt::ControlModifier);
            if (m_saveOnReturnKey && controlPressed ) {
                keyevent->setModifiers(mods & ~Qt::ControlModifier);
            } else if ( m_saveOnReturnKey || controlPressed ) {
                saveAndExit();
                return;
            }
        }
    }

    QTextEdit::keyPressEvent(event);
}

bool ItemEditorWidget::canInsertFromMimeData(const QMimeData *source) const
{
    return source->hasImage() || QTextEdit::canInsertFromMimeData(source);
}

void ItemEditorWidget::insertFromMimeData(const QMimeData *source)
{
    const QString mime = findImageFormat(*source);

    if (!mime.isEmpty()) {
        const QByteArray imageData = source->data(mime);
        textCursor().insertHtml(
                    "<img src=\"data:" + mime + ";base64," + imageData.toBase64() + "\" />");
    } else {
        QTextEdit::insertFromMimeData(source);
    }
}

void ItemEditorWidget::saveAndExit()
{
    emit save();
    emit invalidate();
}

void ItemEditorWidget::changeSelectionFont()
{
    QTextCursor tc = textCursor();
    QTextCharFormat format = tc.charFormat();

    QFontDialog dialog(this);
    dialog.setOptions(dialog.options() | QFontDialog::DontUseNativeDialog);
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
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
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
    dialog.setOptions(dialog.options() | QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
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

QWidget *ItemEditorWidget::createToolbar(QWidget *parent)
{
    auto toolBar = new QToolBar(parent);

    QAction *act;
    act = new QAction( iconSave(), tr("Save"), this );
    toolBar->addAction(act);
    act->setToolTip( tr("Save Item (<strong>F2</strong>)") );
    act->setShortcut( QKeySequence(tr("F2", "Shortcut to save item editor changes")) );
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::saveAndExit );

    act = new QAction( iconCancel(), tr("Cancel"), this );
    toolBar->addAction(act);
    act->setToolTip( tr("Cancel Editing and Revert Changes") );
    act->setShortcut( QKeySequence(tr("Escape", "Shortcut to revert item editor changes")) );
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::cancel );

    auto doc = document();

    toolBar->addSeparator();

    act = new QAction( iconUndo(), tr("Undo"), this );
    toolBar->addAction(act);
    act->setShortcut(QKeySequence::Undo);
    act->setEnabled(false);
    connect( act, &QAction::triggered, doc, static_cast<void (QTextDocument::*)()>(&QTextDocument::undo) );
    connect( doc, &QTextDocument::undoAvailable, act, &QAction::setEnabled );

    act = new QAction( iconRedo(), tr("Redo"), this );
    toolBar->addAction(act);
    act->setShortcut(QKeySequence::Redo);
    act->setEnabled(false);
    connect( act, &QAction::triggered, doc, static_cast<void (QTextDocument::*)()>(&QTextDocument::redo) );
    connect( doc, &QTextDocument::redoAvailable, act, &QAction::setEnabled );

    toolBar->addSeparator();

    act = new QAction( iconFont(), tr("Font"), this );
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::changeSelectionFont );

    act = new QAction( iconBold(), tr("Bold"), this );
    toolBar->addAction(act);
    act->setShortcut( QKeySequence::Bold );
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleBoldText );

    act = new QAction( iconItalic(), tr("Italic"), this );
    toolBar->addAction(act);
    act->setShortcut( QKeySequence::Italic );
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleItalicText );

    act = new QAction( iconUnderline(), tr("Underline"), this );
    toolBar->addAction(act);
    act->setShortcut( QKeySequence::Underline );
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleUnderlineText );

    act = new QAction( iconStrikethrough(), tr("Strikethrough"), this );
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleStrikethroughText );

    toolBar->addSeparator();

    act = new QAction( iconForeground(), tr("Foreground"), this );
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::setForeground );

    act = new QAction( iconBackground(), tr("Background"), this );
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::setBackground );

    toolBar->addSeparator();

    act = new QAction( iconEraseStyle(), tr("Erase Style"), this );
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::eraseStyle );

    toolBar->addSeparator();

    act = new QAction( iconSearch(), tr("Search"), this );
    act->setShortcuts(QKeySequence::Find);
    toolBar->addAction(act);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::searchRequest );

    return toolBar;
}

void ItemEditorWidget::search(bool backwards)
{
    if ( !m_re.isValid() )
        return;

    auto tc = textCursor();
    if ( tc.isNull() )
        return;

    QTextDocument::FindFlags flags;
    if (backwards)
        flags = QTextDocument::FindBackward;

    auto tc2 = tc.document()->find(m_re, tc, flags);
    if (tc2.isNull()) {
        tc2 = tc;
        if (backwards)
            tc2.movePosition(QTextCursor::End);
        else
            tc2.movePosition(QTextCursor::Start);
        tc2 = tc.document()->find(m_re, tc2, flags);
    }

    if (!tc2.isNull())
        setTextCursor(tc2);
}

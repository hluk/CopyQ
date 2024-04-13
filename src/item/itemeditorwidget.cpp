// SPDX-License-Identifier: GPL-3.0-or-later

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

QAction *addMenuItem(const MenuItem &menuItem, QToolBar *toolBar, ItemEditorWidget *parent)
{
    QAction *act = new QAction( getIcon(menuItem.iconName, menuItem.iconId), menuItem.text, parent );
    act->setShortcuts(menuItem.shortcuts);
    toolBar->addAction(act);
    return act;
}

} // namespace

ItemEditorWidget::ItemEditorWidget(const QModelIndex &index, const QString &format, QWidget *parent)
    : QTextEdit(parent)
    , m_index(index)
    , m_saveOnReturnKey(false)
    , m_format(format)
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
    if (m_format == mimeText) {
        setTextData( &data, toPlainText(), mimeText );
        if ( containsRichText(*document()) )
            setTextData( &data, toHtml(), mimeHtml );
    } else {
        setTextData( &data, toPlainText(), m_format );
    }
    return data;
}

void ItemEditorWidget::search(const ItemFilterPtr &filter)
{
    if ( !filter || filter->matchesNone() )
        return;

    auto tc = textCursor();
    tc.setPosition(tc.selectionStart());
    setTextCursor(tc);
    m_filter = filter;
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

QWidget *ItemEditorWidget::createToolbar(QWidget *parent, const MenuItems &menuItems)
{
    auto toolBar = new QToolBar(parent);

    QAction *act;
    act = addMenuItem(menuItems[Actions::Editor_Save], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::saveAndExit );

    act = addMenuItem(menuItems[Actions::Editor_Cancel], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::cancel );

    auto doc = document();

    toolBar->addSeparator();

    act = addMenuItem(menuItems[Actions::Editor_Undo], toolBar, this);
    act->setEnabled(false);
    connect( act, &QAction::triggered, doc, static_cast<void (QTextDocument::*)()>(&QTextDocument::undo) );
    connect( doc, &QTextDocument::undoAvailable, act, &QAction::setEnabled );

    act = addMenuItem(menuItems[Actions::Editor_Redo], toolBar, this);
    act->setEnabled(false);
    connect( act, &QAction::triggered, doc, static_cast<void (QTextDocument::*)()>(&QTextDocument::redo) );
    connect( doc, &QTextDocument::redoAvailable, act, &QAction::setEnabled );

    toolBar->addSeparator();

    act = addMenuItem(menuItems[Actions::Editor_Font], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::changeSelectionFont );

    act = addMenuItem(menuItems[Actions::Editor_Bold], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleBoldText );

    act = addMenuItem(menuItems[Actions::Editor_Italic], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleItalicText );

    act = addMenuItem(menuItems[Actions::Editor_Underline], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleUnderlineText );

    act = addMenuItem(menuItems[Actions::Editor_Strikethrough], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::toggleStrikethroughText );

    toolBar->addSeparator();

    act = addMenuItem(menuItems[Actions::Editor_Foreground], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::setForeground );

    act = addMenuItem(menuItems[Actions::Editor_Background], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::setBackground );

    toolBar->addSeparator();

    act = addMenuItem(menuItems[Actions::Editor_EraseStyle], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::eraseStyle );

    toolBar->addSeparator();

    act = addMenuItem(menuItems[Actions::Editor_Search], toolBar, this);
    connect( act, &QAction::triggered,
             this, &ItemEditorWidget::searchRequest );

    return toolBar;
}

void ItemEditorWidget::search(bool backwards)
{
    if (m_filter)
        m_filter->search(this, backwards);
}

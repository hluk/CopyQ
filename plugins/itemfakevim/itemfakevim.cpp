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

#include "itemfakevim.h"
#include "ui_itemfakevimsettings.h"

#include "tests/itemfakevimtests.h"
#include "common/contenttype.h"

#include "fakevim/fakevimhandler.h"

using namespace FakeVim::Internal;

#include <QIcon>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QMetaMethod>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QtPlugin>

#define EDITOR(s) (m_textEdit ? m_textEdit->s : m_plainTextEdit->s)

namespace {

const char propertyWrapped[] = "CopyQ_fakevim_wrapped";

// The same method for rendering document doesn't work for QPlainTextEdit.
// This is simplified code from Qt source code.
void drawPlainTextDocument(
        QPlainTextEdit *textEdit,
        const QAbstractTextDocumentLayout::PaintContext context,
        QPainter *painter)
{
    // WORKAROUND: Access protected members of QPlainTextEdit.
    class PlainTextEdit final : public QPlainTextEdit {
    public:
        static QPointF getContentOffset(QPlainTextEdit *edit) {
            return (edit->*(&PlainTextEdit::contentOffset))();
        }
        static QTextBlock getFirstVisibleBlock(QPlainTextEdit *edit) {
            return (edit->*(&PlainTextEdit::firstVisibleBlock))();
        }
    };
    QPointF offset = PlainTextEdit::getContentOffset(textEdit);
    QTextBlock block = PlainTextEdit::getFirstVisibleBlock(textEdit);

    QTextDocument *doc = textEdit->document();
    const auto documentLayout = doc->documentLayout();

    painter->setBrushOrigin(offset);

    while (block.isValid()) {
        const QRectF r = documentLayout->blockBoundingRect(block).translated(offset);
        if (block.isVisible()) {
            QTextLayout *layout = block.layout();

            QVector<QTextLayout::FormatRange> selections;
            int blpos = block.position();
            int bllen = block.length();
            for (int i = 0; i < context.selections.size(); ++i) {
                const QAbstractTextDocumentLayout::Selection &range = context.selections.at(i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;
                if (selStart < bllen && selEnd > 0
                        && selEnd > selStart) {
                    QTextLayout::FormatRange o;
                    o.start = selStart;
                    o.length = selEnd - selStart;
                    o.format = range.format;
                    selections.append(o);
                } else if (!range.cursor.hasSelection() && range.format.hasProperty(QTextFormat::FullWidthSelection)
                           && block.contains(range.cursor.position())) {
                    // for full width selections we don't require an actual selection, just
                    // a position to specify the line. that's more convenience in usage.
                    QTextLayout::FormatRange o;
                    QTextLine l = layout->lineForTextPosition(range.cursor.position() - blpos);
                    o.start = l.textStart();
                    o.length = l.textLength();
                    if (o.start + o.length == bllen - 1)
                        ++o.length; // include newline
                    o.format = range.format;
                    selections.append(o);
                }
            }

            layout->draw(painter, offset, selections);
        }

        offset.ry() += r.height();
        if (offset.y() > context.clip.bottom())
            break;

        block = block.next();
    }
}

class TextEditWrapper final : public QObject
{
public:
    explicit TextEditWrapper(QAbstractScrollArea *editor)
        : QObject(editor)
        , m_textEditWidget(editor)
        , m_textEdit(qobject_cast<QTextEdit *>(editor))
        , m_plainTextEdit(qobject_cast<QPlainTextEdit *>(editor))
        , m_handler(new FakeVimHandler(editor, nullptr))
        , m_hasBlockSelection(false)
    {
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        if (m_textEdit) {
            connect( m_textEdit, &QTextEdit::selectionChanged,
                     this, &TextEditWrapper::onSelectionChanged );
            connect( m_textEdit, &QTextEdit::cursorPositionChanged,
                     this, &TextEditWrapper::onSelectionChanged );
        } else if (m_plainTextEdit) {
            connect( m_plainTextEdit, &QPlainTextEdit::selectionChanged,
                     this, &TextEditWrapper::onSelectionChanged );
            connect( m_plainTextEdit, &QPlainTextEdit::cursorPositionChanged,
                     this, &TextEditWrapper::onSelectionChanged );
        }

        setLineWrappingEnabled(true);

        editor->viewport()->installEventFilter(this);

        auto completer = editor->findChild<QObject*>("CommandCompleter");
        if (completer) {
            m_completerPopup = completer->property("popup").value<QWidget*>();
            Q_ASSERT(m_completerPopup);
            if (m_completerPopup)
                m_completerPopup->installEventFilter(this);
        }
    }

    ~TextEditWrapper()
    {
        m_handler->disconnectFromEditor();
        m_handler->deleteLater();
    }

    void install()
    {
        m_handler->installEventFilter();
        m_handler->setupWidget();
        m_handler->enterCommandMode();
    }

    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        // Handle completion popup.
        if (obj == m_completerPopup) {
            if ( ev->type() == QEvent::KeyPress ) {
                const auto kev = static_cast<QKeyEvent *>(ev);
                const auto key = kev->key();
                const auto mods = kev->modifiers();
                if (mods.testFlag(Qt::ControlModifier)) {
                    if (key == Qt::Key_N || key == Qt::Key_P) {
                        const auto key2 = key == Qt::Key_N ? Qt::Key_Down : Qt::Key_Up;
                        QKeyEvent kev2(QEvent::KeyPress, key2, Qt::NoModifier);
                        QCoreApplication::sendEvent(obj, &kev2);
                        return true;
                    }
                }

                switch (key) {
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    return false;

                case Qt::Key_Down:
                case Qt::Key_Up:
                case Qt::Key_PageDown:
                case Qt::Key_PageUp:
                    return false;

                default:
                    QCoreApplication::sendEvent(m_textEditWidget, ev);
                    return true;
                }
            }
            return false;
        }

        if ( ev->type() != QEvent::Paint )
            return false;

        QWidget *viewport = editor()->viewport();

        QPaintEvent *e = static_cast<QPaintEvent*>(ev);

        const QRect r = e->rect();

        QPainter painter(viewport);

        const QTextCursor tc = EDITOR(textCursor());

        m_context.cursorPosition = -1;
        m_context.palette = editor()->palette();

        const int h = m_textEdit ? horizontalOffset() : 0;
        const int v = m_textEdit ? verticalOffset() : 0;
        m_context.clip = r.translated(h, v);

        painter.save();

        // Draw base and text.
        painter.translate(-h, -v);
        paintDocument(&painter);

        // Draw block selection.
        if ( hasBlockSelection() ) {
            QRect rect;
            QTextCursor tc2 = tc;
            tc2.setPosition(tc.position());
            rect = EDITOR(cursorRect(tc2));
            tc2.setPosition(tc.anchor());
            rect = rect.united( EDITOR(cursorRect(tc2)) );

            m_context.palette.setColor(QPalette::Base, m_context.palette.color(QPalette::Highlight));
            m_context.palette.setColor(QPalette::Text, m_context.palette.color(QPalette::HighlightedText));

            m_context.clip = rect.translated(h, v);

            paintDocument(&painter);
        }

        painter.restore();

        // Draw text cursor.
        QRect rect = EDITOR(cursorRect());

        if (EDITOR(overwriteMode()) || hasBlockSelection() ) {
            QTextCursor tc2 = tc;
            tc2.movePosition(QTextCursor::Right);
            const QRect nextRect = EDITOR(cursorRect(tc2));
            const int w = nextRect.left() - rect.left();
            if (w > 3) {
                rect.setWidth(w);
            } else {
                const QFontMetrics fm(editor()->font());
                rect.setWidth( fm.averageCharWidth() );
            }
        } else {
            rect.setWidth(2);
            rect.adjust(-1, 0, 0, 0);
        }

        if ( hasBlockSelection() ) {
            int from = tc.positionInBlock();
            int to = tc.anchor() - tc.document()->findBlock(tc.anchor()).position();
            if (from > to)
                rect.moveLeft(rect.left() - rect.width());
        }

        painter.setCompositionMode(QPainter::CompositionMode_Difference);
        const auto color = editor()->hasFocus() ? Qt::white : Qt::darkGray;
        painter.fillRect(rect, color);

        if (!hasBlockSelection() && m_cursorRect.width() != rect.width())
            viewport->update();

        m_cursorRect = rect;

        return true;
    }

    FakeVimHandler &fakeVimHandler() { return *m_handler; }

    void highlightMatches(const QString &pattern)
    {
        QTextCursor cur = EDITOR(textCursor());

        Selection selection;
        selection.format.setBackground(Qt::yellow);
        selection.format.setForeground(Qt::black);

        // Highlight matches.
        QTextDocument *doc = document();
        QRegularExpression re(pattern);
        cur = doc->find(re);

        m_searchSelection.clear();

        int a = cur.position();
        while ( !cur.isNull() ) {
            if ( cur.hasSelection() ) {
                selection.cursor = cur;
                m_searchSelection.append(selection);
            } else {
                cur.movePosition(QTextCursor::NextCharacter);
            }
            cur = doc->find(re, cur);
            int b = cur.position();
            if (a == b) {
                cur.movePosition(QTextCursor::NextCharacter);
                cur = doc->find(re, cur);
                b = cur.position();
                if (a == b) break;
            }
            a = b;
        }

        updateSelections();
    }

    void setBlockSelection(bool on)
    {
        m_hasBlockSelection = on;
        m_selection.clear();
        updateSelections();
    }

    bool hasBlockSelection() const
    {
        return m_hasBlockSelection;
    }

    void setTextCursor(const QTextCursor &tc)
    {
        EDITOR(setTextCursor(tc));
    }

    QTextCursor textCursor() const
    {
        return EDITOR(textCursor());
    }

    QTextDocument *document() const
    {
        return EDITOR(document());
    }

    QAbstractScrollArea *editor() const { return m_textEditWidget; }

    void setLineWrappingEnabled(bool enable)
    {
        if (m_textEdit)
            m_textEdit->setLineWrapMode(enable ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
        else if (m_plainTextEdit)
            m_plainTextEdit->setLineWrapMode(enable ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    }

private:
    void onSelectionChanged() {
        m_hasBlockSelection = false;
        m_selection.clear();

        Selection selection;

        const QPalette pal = editor()->palette();
        selection.format.setBackground( pal.color(QPalette::Highlight) );
        selection.format.setForeground( pal.color(QPalette::HighlightedText) );
        selection.cursor = EDITOR(textCursor());
        if ( selection.cursor.hasSelection() )
            m_selection.append(selection);

        updateSelections();
    }

    int horizontalOffset() const
    {
        QScrollBar *hbar = editor()->horizontalScrollBar();
        return editor()->isRightToLeft() ? (hbar->maximum() - hbar->value()) : hbar->value();
    }

    int verticalOffset() const
    {
        return editor()->verticalScrollBar()->value();
    }

    void paintDocument(QPainter *painter)
    {
        painter->setClipRect(m_context.clip);
        painter->fillRect(m_context.clip, m_context.palette.base());
        if (m_textEdit)
            document()->documentLayout()->draw(painter, m_context);
        else if (m_plainTextEdit)
            drawPlainTextDocument(m_plainTextEdit, m_context, painter);
    }

    void updateSelections()
    {
        m_context.selections.clear();
        m_context.selections.reserve( m_searchSelection.size() + m_selection.size() );
        m_context.selections << m_searchSelection << m_selection;
        editor()->viewport()->update();
    }

    QAbstractScrollArea *m_textEditWidget;
    QTextEdit *m_textEdit;
    QPlainTextEdit *m_plainTextEdit;
    FakeVimHandler *m_handler;
    QObject *m_completerPopup = nullptr;
    QRect m_cursorRect;

    bool m_hasBlockSelection;

    using Selection = QAbstractTextDocumentLayout::Selection;
    using SelectionList = QVector<Selection>;
    SelectionList m_searchSelection;
    SelectionList m_selection;

    QAbstractTextDocumentLayout::PaintContext m_context;
};

class Proxy final : public QObject
{
public:
    Proxy(TextEditWrapper *editorWidget, QStatusBar *statusBar, QObject *parent = nullptr)
      : QObject(parent)
      , m_editorWidget(editorWidget)
      , m_statusBar(statusBar)
      , m_statusBarIcon(new QLabel(statusBar))
    {
        m_statusBar->addPermanentWidget(m_statusBarIcon);
        m_statusBarIcon->show();
    }

    void changeStatusData(const QString &info)
    {
        m_statusData = info;
        updateStatusBar();
    }

    void highlightMatches(const QString &pattern)
    {
        m_editorWidget->highlightMatches(pattern);
    }

    void setStatusIcon(QStyle::StandardPixmap standardPixmap)
    {
        const auto icon = m_statusBarIcon->style()->standardIcon(standardPixmap);
        const auto maxHeight = m_statusBarIcon->contentsRect().height();
        const auto window = m_statusBarIcon->windowHandle();

        auto sizes = icon.availableSizes();
        std::sort(std::begin(sizes), std::end(sizes), [](const QSize &lhs, const QSize &rhs){
            return lhs.height() > rhs.height();
        });

        const auto it = std::lower_bound(
            sizes.begin(), sizes.end(), maxHeight,
            [](const QSize &size, int height){
                return size.height() > height;
            });
        const auto size = it == sizes.end() ? icon.actualSize(window, QSize(maxHeight, maxHeight)) : *it;
        const auto pixmap = icon.pixmap(window, size);
        m_statusBarIcon->setPixmap(pixmap);
    }

    void changeStatusMessage(const QString &contents, int cursorPos, int messageLevel)
    {
        m_statusMessage = cursorPos == -1 ? contents
            : contents.left(cursorPos) + QChar(10073) + contents.mid(cursorPos);

        if (messageLevel == MessageWarning)
            setStatusIcon(QStyle::SP_MessageBoxWarning);
        else if (messageLevel == MessageError)
            setStatusIcon(QStyle::SP_MessageBoxCritical);
        else
            m_statusBarIcon->clear();

        updateStatusBar();
    }

    void changeExtraInformation(const QString &info)
    {
        QMessageBox::information(m_editorWidget->editor(), tr("Information"), info);
    }

    void updateStatusBar()
    {
        int slack = 80 - m_statusMessage.size() - m_statusData.size();
        QString msg = m_statusMessage + QString(slack, QLatin1Char(' ')) + m_statusData;
        m_statusBar->showMessage(msg);
    }

    void handleExCommand(bool *handled, const ExCommand &cmd)
    {
        if (cmd.cmd == "set") {
            QString arg = cmd.args;
            bool enable = !arg.startsWith("no");
            if (enable)
                arg.remove(0, 2);
            *handled = setOption(arg, enable);
        } else if ( wantSaveAndQuit(cmd) ) {
            // :wq
            saveAndClose();
            *handled = true;
        } else if ( wantSave(cmd) ) {
            save(); // :w
            *handled = true;
        } else if ( wantQuit(cmd) ) {
            if (cmd.hasBang)
                invalidate(); // :q!
            else
                cancel(); // :q
            *handled = true;
        } else {
            *handled = false;
        }
    }

    void requestSetBlockSelection(const QTextCursor &cursor)
    {
        m_editorWidget->setTextCursor(cursor);
        m_editorWidget->setBlockSelection(true);
    }

    void requestDisableBlockSelection()
    {
        m_editorWidget->setBlockSelection(false);
    }

    void requestBlockSelection(QTextCursor *cursor)
    {
        *cursor = m_editorWidget->textCursor();
        m_editorWidget->setBlockSelection(true);
    }

private:
    bool emitEditorSignal(const char *signal)
    {
        const auto editor = m_editorWidget->editor();
        const QMetaObject *metaObject = editor->metaObject();
        const int i = metaObject->indexOfSignal(signal);
        if (i == -1)
            return false;
        metaObject->method(i).invoke(editor);
        return true;
    }

    void clickDialogButton(QDialogButtonBox::StandardButton standardButton)
    {
        const auto window = m_editorWidget->editor()->window();
        const auto buttonBox = window->findChild<QDialogButtonBox*>();
        QPushButton *button = buttonBox->button(standardButton);
        if (button)
            button->click();
    }

    void save()
    {
        if ( !emitEditorSignal("save()") )
            clickDialogButton(QDialogButtonBox::Apply);
    }

    void saveAndClose()
    {
        if ( !emitEditorSignal("save()") || !emitEditorSignal("cancel()") )
            clickDialogButton(QDialogButtonBox::Ok);
    }

    void cancel()
    {
        if ( !emitEditorSignal("cancel()") )
            clickDialogButton(QDialogButtonBox::Cancel);
    }

    void invalidate()
    {
        if ( !emitEditorSignal("invalidate()") )
            clickDialogButton(QDialogButtonBox::Cancel);
    }

    bool wantSaveAndQuit(const ExCommand &cmd)
    {
        return cmd.cmd == "wq";
    }

    bool wantSave(const ExCommand &cmd)
    {
        return cmd.matches("w", "write") || cmd.matches("wa", "wall");
    }

    bool wantQuit(const ExCommand &cmd)
    {
        return cmd.matches("q", "quit") || cmd.matches("qa", "qall");
    }

    bool setOption(const QString &option, bool enable)
    {
        if (option == "linebreak" || option == "lbr")
            m_editorWidget->setLineWrappingEnabled(enable);
        else
            return false;
        return true;
    }

    TextEditWrapper *m_editorWidget;
    QStatusBar *m_statusBar;
    QLabel *m_statusBarIcon;
    QString m_statusMessage;
    QString m_statusData;
};

void connectSignals(FakeVimHandler *handler, Proxy *proxy)
{
    handler->commandBufferChanged.connect(
        [proxy](const QString &msg, int cursorPos, int, int messageLevel) {
            proxy->changeStatusMessage(msg, cursorPos, messageLevel);
        }
    );
    handler->extraInformationChanged.connect(
        [proxy](const QString &msg) {
            proxy->changeExtraInformation(msg);
        }
    );
    handler->statusDataChanged.connect(
        [proxy](const QString &msg) {
            proxy->changeStatusData(msg);
        }
    );
    handler->highlightMatches.connect(
        [proxy](const QString &needle) {
            proxy->highlightMatches(needle);
        }
    );
    handler->handleExCommandRequested.connect(
        [proxy](bool *handled, const ExCommand &cmd) {
            proxy->handleExCommand(handled, cmd);
        }
    );
    handler->requestSetBlockSelection.connect(
        [proxy](const QTextCursor &cursor) {
            proxy->requestSetBlockSelection(cursor);
        }
    );
    handler->requestDisableBlockSelection.connect(
        [proxy]() {
            proxy->requestDisableBlockSelection();
        }
    );
    handler->requestBlockSelection.connect(
        [proxy](QTextCursor *cursor) {
            proxy->requestBlockSelection(cursor);
        }
    );
}

bool installEditor(QAbstractScrollArea *textEdit, const QString &sourceFileName, ItemFakeVimLoader *loader)
{
    auto wrapper = new TextEditWrapper(textEdit);

    // Position text cursor at the beginning of text instead of selecting all.
    wrapper->setTextCursor( QTextCursor(wrapper->document()) );

    QStatusBar *statusBar = new QStatusBar(textEdit);
    statusBar->setObjectName("item_child");

    const auto layout = textEdit->parentWidget()->layout();
    if (layout)
        layout->addWidget(statusBar);
    statusBar->setFont(textEdit->font());

    // Connect slots to FakeVimHandler signals.
    auto proxy = new Proxy(wrapper, statusBar, wrapper);
    connectSignals( &wrapper->fakeVimHandler(), proxy );

    wrapper->install();

    if (!sourceFileName.isEmpty())
        wrapper->fakeVimHandler().handleCommand("source " + sourceFileName);

    QObject::connect(loader, &ItemFakeVimLoader::deleteAllWrappers, wrapper, &QObject::deleteLater);
    QObject::connect(loader, &ItemFakeVimLoader::deleteAllWrappers, statusBar, &QObject::deleteLater);
    QObject::connect(loader, &ItemFakeVimLoader::deleteAllWrappers, textEdit, [textEdit](){
        textEdit->setProperty(propertyWrapped, false);
    });

    return true;
}

template <typename TextEdit>
bool installEditor(QObject *obj, const QString &sourceFileName, ItemFakeVimLoader *loader)
{
    auto textEdit = qobject_cast<TextEdit *>(obj);
    return textEdit && !textEdit->isReadOnly() && installEditor(textEdit, sourceFileName, loader);
}

} // namespace

ItemFakeVimLoader::ItemFakeVimLoader()
{
}

ItemFakeVimLoader::~ItemFakeVimLoader() = default;

QVariant ItemFakeVimLoader::icon() const
{
    return QIcon(":/fakevim/fakevim.png");
}

void ItemFakeVimLoader::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateCurrentlyEnabledState();
}

QVariantMap ItemFakeVimLoader::applySettings()
{
    QVariantMap settings;
    settings["really_enable"] = m_reallyEnabled = ui->checkBoxEnable->isChecked();
    settings["source_file"] = m_sourceFileName = ui->lineEditSourceFileName->text();

    return settings;
}

void ItemFakeVimLoader::loadSettings(const QVariantMap &settings)
{
    m_reallyEnabled = settings.value("really_enable", false).toBool();
    m_sourceFileName = settings.value("source_file").toString();
    updateCurrentlyEnabledState();
}

QWidget *ItemFakeVimLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemFakeVimSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->checkBoxEnable->setChecked(m_reallyEnabled);
    ui->lineEditSourceFileName->setText(m_sourceFileName);

    return w;
}

QObject *ItemFakeVimLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QVariantMap settings;
    settings["really_enable"] = true;
    settings["source_file"] = QString(ItemFakeVimTests::fileNameToSource());
    QObject *tests = new ItemFakeVimTests(test);
    tests->setProperty("CopyQ_test_settings", settings);
    return tests;
#else
    Q_UNUSED(test);
    return nullptr;
#endif
}

bool ItemFakeVimLoader::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Show)
        wrapEditWidget(watched);

    return false;
}

void ItemFakeVimLoader::updateCurrentlyEnabledState()
{
    if ( qobject_cast<QGuiApplication*>(qApp) == nullptr )
        return;

    const bool enable = m_enabled && m_reallyEnabled;
    if (m_currentlyEnabled == enable)
        return;

    if (enable) {
        // WORKAROUND: Disallow blinking text cursor application-wide
        // (unfortunately, there doesn't seem other way to do this).
        m_oldCursorFlashTime = qApp->cursorFlashTime();
        qApp->setCursorFlashTime(0);

        qApp->installEventFilter(this);

        for (auto topLevel : qApp->topLevelWidgets()) {
            for ( auto textEdit : topLevel->findChildren<QTextEdit*>() )
                wrapEditWidget(textEdit);
            for ( auto textEdit : topLevel->findChildren<QPlainTextEdit*>() )
                wrapEditWidget(textEdit);
        }
    } else {
        emit deleteAllWrappers();

        qApp->removeEventFilter(this);

        qApp->setCursorFlashTime(m_oldCursorFlashTime);
    }

    m_currentlyEnabled = enable;
}

void ItemFakeVimLoader::wrapEditWidget(QObject *obj)
{
    if ( !obj->property(propertyWrapped).toBool()
         && ( installEditor<QTextEdit>(obj, m_sourceFileName, this)
              || installEditor<QPlainTextEdit>(obj, m_sourceFileName, this) )
         )
    {
        obj->setProperty(propertyWrapped, true);
    }
}

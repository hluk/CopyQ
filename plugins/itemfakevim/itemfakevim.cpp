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

#include "itemfakevim.h"
#include "ui_itemfakevimsettings.h"

#include "tests/itemfakevimtests.h"
#include "common/contenttype.h"

#include "fakevim/fakevimhandler.h"

using namespace FakeVim::Internal;

#include <QIcon>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QtPlugin>

namespace {

class TextEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextEditWidget(QTextEdit *editor, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_textEdit(editor)
        , m_handler(new FakeVimHandler(editor, nullptr))
        , m_hasBlockSelection(false)
    {
        auto layout = new QVBoxLayout(this);
        layout->setMargin(0);
        layout->addWidget(editor);

        setFocusProxy(editor);

        m_handler->installEventFilter();
        m_handler->setupWidget();

        connect( editor, SIGNAL(selectionChanged()),
                 this, SLOT(onSelectionChanged()) );
        connect( editor, SIGNAL(cursorPositionChanged()),
                 this, SLOT(onSelectionChanged()) );

        setLineWrappingEnabled(true);

        editor->viewport()->installEventFilter(this);

        editor->setStyleSheet("QTextEdit{background:transparent}");
    }

    ~TextEditWidget()
    {
        m_handler->disconnectFromEditor();
        m_handler->deleteLater();
    }

    bool eventFilter(QObject *, QEvent *ev) override
    {
        if ( ev->type() != QEvent::Paint )
            return false;

        QWidget *viewport = editor()->viewport();

        QPaintEvent *e = static_cast<QPaintEvent*>(ev);

        const QRect r = e->rect();

        QPainter painter(viewport);

        const QTextCursor tc = editor()->textCursor();

        m_context.cursorPosition = -1;
        m_context.palette = palette();

        const int h = horizontalOffset();
        const int v = verticalOffset();
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
            rect = editor()->cursorRect(tc2);
            tc2.setPosition(tc.anchor());
            rect = rect.united( editor()->cursorRect(tc2) );

            m_context.palette.setColor(QPalette::Base, m_context.palette.color(QPalette::Highlight));
            m_context.palette.setColor(QPalette::Text, m_context.palette.color(QPalette::HighlightedText));

            m_context.clip = rect.translated(h, v);

            paintDocument(&painter);
        }

        painter.restore();

        // Draw text cursor.
        QRect rect = editor()->cursorRect();

        if (editor()->overwriteMode() || hasBlockSelection() ) {
            QFontMetrics fm(font());
            QChar c = editor()->document()->characterAt( tc.position() );
            rect.setWidth( fm.width(c) );
            if (rect.width() == 0)
                rect.setWidth( fm.averageCharWidth() );
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
        painter.fillRect(rect, Qt::white);

        if (!hasBlockSelection() && m_cursorRect.width() != rect.width())
            viewport->update();

        m_cursorRect = rect;

        return true;
    }

    FakeVimHandler &fakeVimHandler() { return *m_handler; }

    void highlightMatches(const QString &pattern)
    {
        QTextCursor cur = editor()->textCursor();

        Selection selection;
        selection.format.setBackground(Qt::yellow);
        selection.format.setForeground(Qt::black);

        // Highlight matches.
        QTextDocument *doc = editor()->document();
        QRegExp re(pattern);
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

    QTextEdit *editor() const { return m_textEdit; }

    void setLineWrappingEnabled(bool enable)
    {
        editor()->setLineWrapMode(enable ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    }

private slots:
    void onSelectionChanged() {
        m_hasBlockSelection = false;
        m_selection.clear();

        Selection selection;

        const QPalette pal = palette();
        selection.format.setBackground( pal.color(QPalette::Highlight) );
        selection.format.setForeground( pal.color(QPalette::HighlightedText) );
        selection.cursor = editor()->textCursor();
        if ( selection.cursor.hasSelection() )
            m_selection.append(selection);

        updateSelections();
    }

private:
    int horizontalOffset() const
    {
        QScrollBar *hbar = editor()->horizontalScrollBar();
        return isRightToLeft() ? (hbar->maximum() - hbar->value()) : hbar->value();
    }

    int verticalOffset() const
    {
        return editor()->verticalScrollBar()->value();
    }

    void paintDocument(QPainter *painter)
    {
        painter->setClipRect(m_context.clip);
        painter->fillRect(m_context.clip, m_context.palette.base());
        editor()->document()->documentLayout()->draw(painter, m_context);
    }

    void updateSelections()
    {
        m_context.selections.clear();
        m_context.selections.reserve( m_searchSelection.size() + m_selection.size() );
        m_context.selections << m_searchSelection << m_selection;
        editor()->viewport()->update();
    }

    QTextEdit *m_textEdit;
    FakeVimHandler *m_handler;
    QRect m_cursorRect;

    bool m_hasBlockSelection;

    using Selection = QAbstractTextDocumentLayout::Selection;
    using SelectionList = QVector<Selection>;
    SelectionList m_searchSelection;
    SelectionList m_selection;

    QAbstractTextDocumentLayout::PaintContext m_context;
};

class Proxy : public QObject
{
    Q_OBJECT

public:
    Proxy(TextEditWidget *editorWidget, QStatusBar *statusBar, QObject *parent = nullptr)
      : QObject(parent), m_editorWidget(editorWidget), m_statusBar(statusBar)
    {}

public slots:
    void changeStatusData(const QString &info)
    {
        m_statusData = info;
        updateStatusBar();
    }

    void highlightMatches(const QString &pattern)
    {
        m_editorWidget->highlightMatches(pattern);
    }

    void changeStatusMessage(const QString &contents, int cursorPos)
    {
        m_statusMessage = cursorPos == -1 ? contents
            : contents.left(cursorPos) + QChar(10073) + contents.mid(cursorPos);
        updateStatusBar();
    }

    void changeExtraInformation(const QString &info)
    {
        QMessageBox::information(m_editorWidget, tr("Information"), info);
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
            emit save();
            emit cancel();
        } else if ( wantSave(cmd) ) {
            emit save(); // :w
        } else if ( wantQuit(cmd) ) {
            if (cmd.hasBang)
                emit invalidate(); // :q!
            else
                emit cancel(); // :q
        } else {
            *handled = false;
            return;
        }

        *handled = true;
    }

    void requestSetBlockSelection(const QTextCursor &cursor)
    {
        m_editorWidget->editor()->setTextCursor(cursor);
        m_editorWidget->setBlockSelection(true);
    }

    void requestDisableBlockSelection()
    {
        m_editorWidget->setBlockSelection(false);
    }

    void requestBlockSelection(QTextCursor *cursor)
    {
        *cursor = m_editorWidget->editor()->textCursor();
        m_editorWidget->setBlockSelection(true);
    }

signals:
    void save();
    void cancel();
    void invalidate();

private:
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

    TextEditWidget *m_editorWidget;
    QStatusBar *m_statusBar;
    QString m_statusMessage;
    QString m_statusData;
};

class Editor : public QWidget
{
    Q_OBJECT

public:
    Editor(QTextEdit *editor, const QString &sourceFileName, QWidget *parent)
        : QWidget(parent)
        , m_editor(new TextEditWidget(editor, this))
    {
        m_editor->editor()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // Create status bar.
        m_statusBar = new QStatusBar(this);

        // Connect slots to FakeVimHandler signals.
        auto proxy = new Proxy(m_editor, m_statusBar, this);
        connectSignals( &m_editor->fakeVimHandler(), proxy );

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_editor);
        layout->addWidget(m_statusBar);
        setFocusProxy(m_editor);

        if (!sourceFileName.isEmpty())
            m_editor->fakeVimHandler().handleCommand("source " + sourceFileName);
    }

    TextEditWidget *textEditWidget() { return m_editor; }

signals:
    void save();
    void cancel();
    void invalidate();

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::PaletteChange) {
            QPalette pal = palette();
            m_editor->setPalette(pal);
            pal.setColor(QPalette::Window, pal.color(QPalette::Base));
            pal.setColor(QPalette::WindowText, pal.color(QPalette::Text));
            m_statusBar->setPalette(pal);
        } else if (event->type() == QEvent::FontChange) {
            m_editor->setFont(font());
            m_editor->editor()->setFont(font());
        }
        return false;
    }

private:
    void connectSignals(FakeVimHandler *handler, Proxy *proxy)
    {
        connect(handler, SIGNAL(commandBufferChanged(QString,int,int,int,QObject*)),
                proxy, SLOT(changeStatusMessage(QString,int)));
        connect(handler, SIGNAL(extraInformationChanged(QString)),
                proxy, SLOT(changeExtraInformation(QString)));
        connect(handler, SIGNAL(statusDataChanged(QString)),
                proxy, SLOT(changeStatusData(QString)));
        connect(handler, SIGNAL(highlightMatches(QString)),
                proxy, SLOT(highlightMatches(QString)));
        connect(handler, SIGNAL(handleExCommandRequested(bool*,ExCommand)),
                proxy, SLOT(handleExCommand(bool*,ExCommand)));
        connect(handler, SIGNAL(requestSetBlockSelection(QTextCursor)),
                proxy, SLOT(requestSetBlockSelection(QTextCursor)));
        connect(handler, SIGNAL(requestDisableBlockSelection()),
                proxy, SLOT(requestDisableBlockSelection()));
        connect(handler, SIGNAL(requestBlockSelection(QTextCursor*)),
                proxy, SLOT(requestBlockSelection(QTextCursor*)));

        connect(proxy, SIGNAL(save()), SIGNAL(save()));
        connect(proxy, SIGNAL(cancel()), SIGNAL(cancel()));
        connect(proxy, SIGNAL(invalidate()), SIGNAL(invalidate()));
    }

    TextEditWidget *m_editor;
    QStatusBar *m_statusBar;
};

QWidget *getItemEditorWidget(QWidget *editor)
{
    Editor *ed = qobject_cast<Editor*>(editor);
    return ed ? ed->textEditWidget()->editor() : editor;
}

} // namespace

ItemFakeVim::ItemFakeVim(ItemWidget *childItem, const QString &sourceFileName)
    : ItemWidget(childItem->widget())
    , m_childItem(childItem)
    , m_sourceFileName(sourceFileName)
{
}

void ItemFakeVim::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);
}

void ItemFakeVim::updateSize(const QSize &maximumSize, int idealWidth)
{
    m_childItem->updateSize(maximumSize, idealWidth);
}

QWidget *ItemFakeVim::createEditor(QWidget *parent) const
{
    QWidget *editor = m_childItem->createEditor(parent);
    QTextEdit *textEdit = qobject_cast<QTextEdit *>(editor);
    if (textEdit)
        return new Editor(textEdit, m_sourceFileName, parent);
    return editor;
}

void ItemFakeVim::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    m_childItem->setEditorData( getItemEditorWidget(editor), index );

    // Position text cursor at the begining of text instead of selecting all.
    auto ed = qobject_cast<Editor*>(editor);
    if (ed) {
        auto textEdit = ed->textEditWidget()->editor();
        textEdit->setTextCursor( QTextCursor(textEdit->document()) );
    }
}

void ItemFakeVim::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    m_childItem->setModelData( getItemEditorWidget(editor), model, index );
}

bool ItemFakeVim::hasChanges(QWidget *editor) const
{
    return m_childItem->hasChanges( getItemEditorWidget(editor) );

}

QObject *ItemFakeVim::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem->createExternalEditor(index, parent);
}

void ItemFakeVim::setTagged(bool tagged)
{
    return m_childItem->setTagged(tagged);
}

ItemFakeVimLoader::ItemFakeVimLoader()
    : m_enabled(false)
{
}

ItemFakeVimLoader::~ItemFakeVimLoader() = default;

QVariant ItemFakeVimLoader::icon() const
{
    return QIcon(":/fakevim/fakevim.png");
}

QVariantMap ItemFakeVimLoader::applySettings()
{
    QVariantMap settings;
    settings["really_enable"] = m_enabled = ui->checkBoxEnable->isChecked();
    settings["source_file"] = m_sourceFileName = ui->lineEditSourceFileName->text();

    return settings;
}

void ItemFakeVimLoader::loadSettings(const QVariantMap &settings)
{
    m_enabled = settings.value("really_enable", false).toBool();
    m_sourceFileName = settings.value("source_file").toString();
}

QWidget *ItemFakeVimLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemFakeVimSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    ui->checkBoxEnable->setChecked(m_enabled);
    ui->lineEditSourceFileName->setText(m_sourceFileName);

    return w;
}

ItemWidget *ItemFakeVimLoader::transform(ItemWidget *itemWidget, const QModelIndex &)
{
    return m_enabled ? new ItemFakeVim(itemWidget, m_sourceFileName) : nullptr;
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

Q_EXPORT_PLUGIN2(itemfakevim, ItemFakeVimLoader)

#include "itemfakevim.moc"

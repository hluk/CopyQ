// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandcompleter.h"
#include "commandsyntaxhighlighter.h"
#include "commandcompleterdocumentation.h"

#include "common/appconfig.h"
#include "common/timer.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTableView>
#include <QTimer>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStringListModel>
#include <QShortcut>

namespace {

const QLatin1String tagObject("obj");
const QLatin1String tagType("type");
const QLatin1String tagProperty("prop");
const QLatin1String tagFunction("fn");
const QLatin1String tagKeyword("kw");

class CommandCompleterModel final : public QStringListModel {
public:
    explicit CommandCompleterModel(QObject *parent)
        : QStringListModel(parent)
    {
        for (const QString &name : scriptableObjects()) {
            if (name.size() > 1 && name[0].isUpper() && name[1].isLower())
                m_doc[name].tag = tagType;
            else
                m_doc[name].tag = tagObject;
        }

        for (const auto &name : scriptableProperties())
            m_doc[name].tag = tagProperty;

        for (const auto &name : scriptableFunctions())
            m_doc[name].tag = tagFunction;

        for (const auto &name : scriptableKeywords())
            m_doc[name].tag = tagKeyword;

        addDocumentation();

        QStringList completionItems = m_doc.keys();
        std::sort(std::begin(completionItems), std::end(completionItems));
        setStringList(completionItems);
    }

    int columnCount(const QModelIndex &) const override
    {
        return 3;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        const int row = index.row();

        if (index.column() == 1) {
            if (role == Qt::DisplayRole || role == Qt::EditRole)
                return documentationForRow(row).tag;

            if (role == Qt::ForegroundRole)
                return QColor(Qt::gray);

            return QVariant();
        }

        if (index.column() == 2) {
            if (role == Qt::DisplayRole || role == Qt::EditRole)
                return documentationForRow(row).doc;

            return QVariant();
        }

        if (role == Qt::ToolTipRole)
            return typeForRow(row);

        return QStringListModel::data(index, role);
    }

private:
    struct ScriptableDocumentation {
        QString tag;
        QString doc;
    };

    void addDocumentation()
    {
        ::addDocumentation(
            [this](const QString &name, const QString &api, const QString &documentation) {
                auto &doc = m_doc[name].doc;
                if (!doc.isEmpty())
                    doc.append("\n");
                doc.append(api + "\n    " + documentation);
            });
    }

    ScriptableDocumentation documentationForRow(int row) const
    {
        const auto index2 = this->index(row, 0);
        const auto text = QStringListModel::data(index2, Qt::EditRole).toString();
        return m_doc.value(text);
    }

    QString typeForRow(int row) const
    {
        const auto tagText = documentationForRow(row).tag;
        if (tagText == tagObject)
            return QStringLiteral("object");
        if (tagText == tagType)
            return QStringLiteral("type");
        if (tagText == tagProperty)
            return QStringLiteral("property");
        if (tagText == tagFunction)
            return QStringLiteral("function");
        if (tagText == tagKeyword)
            return QStringLiteral("keyword");
        return tagText;
    }

    QHash<QString, ScriptableDocumentation> m_doc;
};

void setUpHeader(QHeaderView *header)
{
    header->hide();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
}

class CommandCompleterPopup final : public QTableView {
public:
    explicit CommandCompleterPopup(QWidget *parent)
        : QTableView(parent)
    {
        setUpHeader(horizontalHeader());
        setUpHeader(verticalHeader());
        setShowGrid(false);
        setContentsMargins(0, 0, 0, 0);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setAlternatingRowColors(true);

        initSingleShotTimer(&m_timerResize, 10, this, &CommandCompleterPopup::updateSize);
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QTableView::showEvent(event);
        m_timerResize.start();
    }

private:
    void updateSize()
    {
        if (!model())
            return;

        const auto margins = contentsMargins();
        const int w = columnsWidth()
                + (verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0)
                + margins.left() + margins.right();
        const int h = rowsHeight(8)
                + (horizontalScrollBar()->isVisible() ? verticalScrollBar()->height() : 0)
                + margins.top() + margins.bottom();
        resize(w, h);
    }

    int columnsWidth() const
    {
        int width = 0;
        for ( int column = 0; column < model()->columnCount(); ++column )
            width += columnWidth(column);
        return width;
    }

    int rowsHeight(int maxRows) const
    {
        int height = 0;
        for ( int row = 0; row < qMin(maxRows, model()->rowCount()); ++row )
            height += rowHeight(row);
        return height;
    }

    QTimer m_timerResize;
};

} // namespace

CommandCompleter::CommandCompleter(QPlainTextEdit *editor)
    : QObject(editor)
    , m_editor(editor)
    , m_completer(new QCompleter(new CommandCompleterModel(this), this))
{
    setObjectName("CommandCompleter");

    m_completer->setWidget(m_editor);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);

    m_completer->setPopup(new CommandCompleterPopup(m_editor));
    m_completer->popup()->installEventFilter(this);

    connect( m_completer, static_cast<void (QCompleter::*)(const QString&)>(&QCompleter::activated),
             this, &CommandCompleter::insertCompletion );

    connect( m_editor, &QPlainTextEdit::textChanged,
             this, &CommandCompleter::onTextChanged );
    connect( m_editor, &QPlainTextEdit::cursorPositionChanged,
             m_completer->popup(), &QWidget::hide );

    auto shortcut = new QShortcut(tr("Ctrl+Space", "Shortcut to show completion menu"), editor);
    connect( shortcut, &QShortcut::activated,
             this, &CommandCompleter::showCompletion );
}

bool CommandCompleter::eventFilter(QObject *, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return false;

    QKeyEvent *e = static_cast<QKeyEvent*>(event);
    QAbstractItemView *popup = m_completer->popup();

    switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        m_completer->setCurrentRow( popup->currentIndex().row() );
        insertCompletion( m_completer->currentCompletion() );
        popup->hide();
        return true;

    case Qt::Key_Escape:
        popup->hide();
        return true;

    default:
        return false;
    }
}

QWidget *CommandCompleter::popup() const
{
    return m_completer->popup();
}

void CommandCompleter::onTextChanged()
{
    updateCompletion(false);
}

void CommandCompleter::updateCompletion(bool forceShow)
{
    const QString completionPrefix = textUnderCursor();

    QAbstractItemView *popup = m_completer->popup();

    if ( !forceShow && completionPrefix.length() < 3 ) {
        popup->hide();
    } else {
        // Don't auto-complete if it's disabled in configuration.
        if ( !forceShow && !AppConfig().option<Config::autocompletion>() )
            return;

        if (completionPrefix != m_completer->completionPrefix()) {
            m_completer->setCompletionPrefix(completionPrefix);
            popup->setCurrentIndex(m_completer->completionModel()->index(0, 0));
        }

        const auto rect = m_editor->cursorRect();
        m_completer->complete(rect);
    }
}

void CommandCompleter::insertCompletion(const QString &completion)
{
    QTextCursor tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::StartOfWord);
    tc.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    tc.insertText(completion);
    m_editor->setTextCursor(tc);
}

void CommandCompleter::showCompletion()
{
    updateCompletion(true);
}

QString CommandCompleter::textUnderCursor() const
{
    auto tc = m_editor->textCursor();
    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
    tc.select(QTextCursor::WordUnderCursor);
    const auto text = tc.selectedText();
    return text;
}

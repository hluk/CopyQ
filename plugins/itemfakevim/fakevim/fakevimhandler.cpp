/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

//
// ATTENTION:
//
// 1 Please do not add any direct dependencies to other Qt Creator code here.
//   Instead emit signals and let the FakeVimPlugin channel the information to
//   Qt Creator. The idea is to keep this file here in a "clean" state that
//   allows easy reuse with any QTextEdit or QPlainTextEdit derived class.
//
// 2 There are a few auto tests located in ../../../tests/auto/fakevim.
//   Commands that are covered there are marked as "// tested" below.
//
// 3 Some conventions:
//
//   Use 1 based line numbers and 0 based column numbers. Even though
//   the 1 based line are not nice it matches vim's and QTextEdit's 'line'
//   concepts.
//
//   Do not pass QTextCursor etc around unless really needed. Convert
//   early to  line/column.
//
//   A QTextCursor is always between characters, whereas vi's cursor is always
//   over a character. FakeVim interprets the QTextCursor to be over the character
//   to the right of the QTextCursor's position().
//
//   A current "region of interest"
//   spans between anchor(), (i.e. the character below anchor()), and
//   position(). The character below position() is not included
//   if the last movement command was exclusive (MoveExclusive).
//

#include "fakevimhandler.h"

#include "fakevimactions.h"
#include "fakevimtr.h"

#include <QDebug>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <QStack>

#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QMimeData>
#include <QSharedPointer>
#include <QDir>

#include <algorithm>
#include <climits>
#include <ctype.h>
#include <functional>

//#define DEBUG_KEY  1
#ifdef DEBUG_KEY
#   define KEY_DEBUG(s) qDebug() << s
#else
#   define KEY_DEBUG(s)
#endif

//#define DEBUG_UNDO  1
#ifdef DEBUG_UNDO
#   define UNDO_DEBUG(s) qDebug() << "REV" << revision() << s
#else
#   define UNDO_DEBUG(s)
#endif

namespace FakeVim {
namespace Internal {

///////////////////////////////////////////////////////////////////////
//
// FakeVimHandler
//
///////////////////////////////////////////////////////////////////////

#define StartOfLine     QTextCursor::StartOfLine
#define EndOfLine       QTextCursor::EndOfLine
#define MoveAnchor      QTextCursor::MoveAnchor
#define KeepAnchor      QTextCursor::KeepAnchor
#define Up              QTextCursor::Up
#define Down            QTextCursor::Down
#define Right           QTextCursor::Right
#define Left            QTextCursor::Left
#define EndOfDocument   QTextCursor::End
#define StartOfDocument QTextCursor::Start
#define NextBlock       QTextCursor::NextBlock

#define ParagraphSeparator QChar::ParagraphSeparator

#define EDITOR(s) (m_textedit ? m_textedit->s : m_plaintextedit->s)


#ifdef Q_OS_DARWIN
#define ControlModifier Qt::MetaModifier
#else
#define ControlModifier Qt::ControlModifier
#endif

/* Clipboard MIME types used by Vim. */
static const QString vimMimeText = "_VIM_TEXT";
static const QString vimMimeTextEncoded = "_VIMENC_TEXT";

using namespace Qt;

/*! A \e Mode represents one of the basic modes of operation of FakeVim.
*/

enum Mode
{
    InsertMode,
    ReplaceMode,
    CommandMode,
    ExMode
};

enum BlockInsertMode
{
    NoneBlockInsertMode,
    AppendBlockInsertMode,
    AppendToEndOfLineBlockInsertMode,
    InsertBlockInsertMode,
    ChangeBlockInsertMode
};

/*! A \e SubMode is used for things that require one more data item
    and are 'nested' behind a \l Mode.
*/
enum SubMode
{
    NoSubMode,
    ChangeSubMode,       // Used for c
    DeleteSubMode,       // Used for d
    FilterSubMode,       // Used for !
    IndentSubMode,       // Used for =
    RegisterSubMode,     // Used for "
    ShiftLeftSubMode,    // Used for <
    ShiftRightSubMode,   // Used for >
    InvertCaseSubMode,   // Used for g~
    DownCaseSubMode,     // Used for gu
    UpCaseSubMode,       // Used for gU
    WindowSubMode,       // Used for Ctrl-w
    YankSubMode,         // Used for y
    ZSubMode,            // Used for z
    CapitalZSubMode,     // Used for Z
    ReplaceSubMode,      // Used for r
    MacroRecordSubMode,  // Used for q
    MacroExecuteSubMode, // Used for @
    CtrlVSubMode,        // Used for Ctrl-v in insert mode
    CtrlRSubMode         // Used for Ctrl-r in insert mode
};

/*! A \e SubSubMode is used for things that require one more data item
    and are 'nested' behind a \l SubMode.
*/
enum SubSubMode
{
    NoSubSubMode,
    FtSubSubMode,          // Used for f, F, t, T.
    MarkSubSubMode,        // Used for m.
    BackTickSubSubMode,    // Used for `.
    TickSubSubMode,        // Used for '.
    TextObjectSubSubMode,  // Used for thing like iw, aW, as etc.
    ZSubSubMode,           // Used for zj, zk
    OpenSquareSubSubMode,  // Used for [{, {(, [z
    CloseSquareSubSubMode, // Used for ]}, ]), ]z
    SearchSubSubMode,
    CtrlVUnicodeSubSubMode // Used for Ctrl-v based unicode input
};

enum VisualMode
{
    NoVisualMode,
    VisualCharMode,
    VisualLineMode,
    VisualBlockMode
};

enum MoveType
{
    MoveExclusive,
    MoveInclusive,
    MoveLineWise
};

/*!
    \enum RangeMode

    The \e RangeMode serves as a means to define how the "Range" between
    the \l cursor and the \l anchor position is to be interpreted.

    \value RangeCharMode   Entered by pressing \key v. The range includes
                           all characters between cursor and anchor.
    \value RangeLineMode   Entered by pressing \key V. The range includes
                           all lines between the line of the cursor and
                           the line of the anchor.
    \value RangeLineModeExclusive Like \l RangeLineMode, but keeps one
                           newline when deleting.
    \value RangeBlockMode  Entered by pressing \key Ctrl-v. The range includes
                           all characters with line and column coordinates
                           between line and columns coordinates of cursor and
                           anchor.
    \value RangeBlockAndTailMode Like \l RangeBlockMode, but also includes
                           all characters in the affected lines up to the end
                           of these lines.
*/

enum EventResult
{
    EventHandled,
    EventUnhandled,
    EventCancelled, // Event is handled but a sub mode was cancelled.
    EventPassedToCore
};

struct CursorPosition
{
    CursorPosition() = default;
    CursorPosition(int block, int column) : line(block), column(column) {}
    explicit CursorPosition(const QTextCursor &tc)
        : line(tc.block().blockNumber()), column(tc.positionInBlock()) {}
    CursorPosition(const QTextDocument *document, int position)
    {
        QTextBlock block = document->findBlock(position);
        line = block.blockNumber();
        column = position - block.position();
    }
    bool isValid() const { return line >= 0 && column >= 0; }
    bool operator>(const CursorPosition &other) const
        { return line > other.line || column > other.column; }
    bool operator==(const CursorPosition &other) const
        { return line == other.line && column == other.column; }
    bool operator!=(const CursorPosition &other) const { return !operator==(other); }

    int line = -1; // Line in document (from 0, folded lines included).
    int column = -1; // Position on line.
};

QDebug operator<<(QDebug ts, const CursorPosition &pos)
{
    return ts << "(line: " << pos.line << ", column: " << pos.column << ")";
}

class Mark final
{
public:
    Mark(const CursorPosition &pos = CursorPosition(), const QString &fileName = QString())
        : m_position(pos), m_fileName(fileName) {}

    bool isValid() const { return m_position.isValid(); }

    bool isLocal(const QString &localFileName) const
    {
        return m_fileName.isEmpty() || m_fileName == localFileName;
    }

    /* Return position of mark within given document.
     * If saved line number is too big, mark position is at the end of document.
     * If line number is in document but column is too big, mark position is at the end of line.
     */
    CursorPosition position(const QTextDocument *document) const
    {
        QTextBlock block = document->findBlockByNumber(m_position.line);
        CursorPosition pos;
        if (block.isValid()) {
            pos.line = m_position.line;
            pos.column = qMax(0, qMin(m_position.column, block.length() - 2));
        } else if (document->isEmpty()) {
            pos.line = 0;
            pos.column = 0;
        } else {
            pos.line = document->blockCount() - 1;
            pos.column = qMax(0, document->lastBlock().length() - 2);
        }
        return pos;
    }

    const QString &fileName() const { return m_fileName; }

    void setFileName(const QString &fileName) { m_fileName = fileName; }

private:
    CursorPosition m_position;
    QString m_fileName;
};
using Marks = QHash<QChar, Mark>;

struct State
{
    State() = default;
    State(int revision, const CursorPosition &position, const Marks &marks,
        VisualMode lastVisualMode, bool lastVisualModeInverted) : revision(revision),
        position(position), marks(marks), lastVisualMode(lastVisualMode),
        lastVisualModeInverted(lastVisualModeInverted) {}

    bool isValid() const { return position.isValid(); }

    int revision = -1;
    CursorPosition position;
    Marks marks;
    VisualMode lastVisualMode = NoVisualMode;
    bool lastVisualModeInverted = false;
};

struct Column
{
    Column(int p, int l) : physical(p), logical(l) {}
    int physical; // Number of characters in the data.
    int logical; // Column on screen.
};

QDebug operator<<(QDebug ts, const Column &col)
{
    return ts << "(p: " << col.physical << ", l: " << col.logical << ")";
}

struct Register
{
    Register() = default;
    Register(const QString &c) : contents(c) {}
    Register(const QString &c, RangeMode m) : contents(c), rangemode(m) {}
    QString contents;
    RangeMode rangemode = RangeCharMode;
};

QDebug operator<<(QDebug ts, const Register &reg)
{
    return ts << reg.contents;
}

struct SearchData
{
    QString needle;
    bool forward = true;
    bool highlightMatches = true;
};

static QString replaceTildeWithHome(QString str)
{
    str.replace("~", QDir::homePath());
    return str;
}

// If string begins with given prefix remove it with trailing spaces and return true.
static bool eatString(const QString &prefix, QString *str)
{
    if (!str->startsWith(prefix))
        return false;
    *str = str->mid(prefix.size()).trimmed();
    return true;
}

static QRegularExpression vimPatternToQtPattern(QString needle, bool ignoreCaseOption, bool smartCaseOption)
{
    /* Transformations (Vim regexp -> QRegularExpression):
     *   \a -> [A-Za-z]
     *   \A -> [^A-Za-z]
     *   \h -> [A-Za-z_]
     *   \H -> [^A-Za-z_]
     *   \l -> [a-z]
     *   \L -> [^a-z]
     *   \o -> [0-7]
     *   \O -> [^0-7]
     *   \u -> [A-Z]
     *   \U -> [^A-Z]
     *   \x -> [0-9A-Fa-f]
     *   \X -> [^0-9A-Fa-f]
     *
     *   \< -> \b
     *   \> -> \b
     *   [] -> \[\]
     *   \= -> ?
     *
     *   (...)  <-> \(...\)
     *   {...}  <-> \{...\}
     *   |      <-> \|
     *   ?      <-> \?
     *   +      <-> \+
     *   \{...} -> {...}
     *
     *   \c - set ignorecase for rest
     *   \C - set noignorecase for rest
     */
    // FIXME: Option smartcase should be used only if search was typed by user.
    bool ignorecase = ignoreCaseOption
        && !(smartCaseOption && needle.contains(QRegularExpression("[A-Z]")));
    QString pattern;
    pattern.reserve(2 * needle.size());

    bool escape = false;
    bool brace = false;
    bool embraced = false;
    bool range = false;
    bool curly = false;
    foreach (const QChar &c, needle) {
        if (brace) {
            brace = false;
            if (c == ']') {
                pattern.append("\\[\\]");
                continue;
            }
            pattern.append('[');
            escape = true;
            embraced = true;
        }
        if (embraced) {
            if (range) {
                QChar c2 = pattern[pattern.size() - 2];
                pattern.remove(pattern.size() - 2, 2);
                pattern.append(c2.toUpper() + '-' + c.toUpper());
                pattern.append(c2.toLower() + '-' + c.toLower());
                range = false;
            } else if (escape) {
                escape = false;
                pattern.append(c);
            } else if (c == '\\') {
                escape = true;
            } else if (c == ']') {
                pattern.append(']');
                embraced = false;
            } else if (c == '-') {
                range = ignorecase && pattern[pattern.size() - 1].isLetter();
                pattern.append('-');
            } else if (c.isLetter() && ignorecase) {
                pattern.append(c.toLower()).append(c.toUpper());
            } else {
                pattern.append(c);
            }
        } else if (QString("(){}+|?").indexOf(c) != -1) {
            if (c == '{') {
                curly = escape;
            } else if (c == '}' && curly) {
                curly = false;
                escape = true;
            }

            if (escape)
                escape = false;
            else
                pattern.append('\\');
            pattern.append(c);
        } else if (escape) {
            // escape expression
            escape = false;
            if (c == '<' || c == '>')
                pattern.append("\\b");
            else if (c == 'a')
                pattern.append("[a-zA-Z]");
            else if (c == 'A')
                pattern.append("[^a-zA-Z]");
            else if (c == 'h')
                pattern.append("[A-Za-z_]");
            else if (c == 'H')
                pattern.append("[^A-Za-z_]");
            else if (c == 'c' || c == 'C')
                ignorecase = (c == 'c');
            else if (c == 'l')
                pattern.append("[a-z]");
            else if (c == 'L')
                pattern.append("[^a-z]");
            else if (c == 'o')
                pattern.append("[0-7]");
            else if (c == 'O')
                pattern.append("[^0-7]");
            else if (c == 'u')
                pattern.append("[A-Z]");
            else if (c == 'U')
                pattern.append("[^A-Z]");
            else if (c == 'x')
                pattern.append("[0-9A-Fa-f]");
            else if (c == 'X')
                pattern.append("[^0-9A-Fa-f]");
            else if (c == '=')
                pattern.append("?");
            else {
                pattern.append('\\');
                pattern.append(c);
            }
        } else {
            // unescaped expression
            if (c == '\\')
                escape = true;
            else if (c == '[')
                brace = true;
            else if (c.isLetter() && ignorecase)
                pattern.append('[').append(c.toLower()).append(c.toUpper()).append(']');
            else
                pattern.append(c);
        }
    }
    if (escape)
        pattern.append('\\');
    else if (brace)
        pattern.append('[');

    return QRegularExpression(pattern);
}

static bool afterEndOfLine(const QTextDocument *doc, int position)
{
    return doc->characterAt(position) == ParagraphSeparator
        && doc->findBlock(position).length() > 1;
}

static void searchForward(QTextCursor *tc, QRegularExpression &needleExp, int *repeat)
{
    const QTextDocument *doc = tc->document();
    const int startPos = tc->position();

    // Search from beginning of line so that matched text is the same.
    tc->movePosition(StartOfLine);

    // forward to current position
    *tc = doc->find(needleExp, *tc);
    while (!tc->isNull() && tc->anchor() < startPos) {
        if (!tc->hasSelection())
            tc->movePosition(Right);
        if (tc->atBlockEnd())
            tc->movePosition(NextBlock);
        *tc = doc->find(needleExp, *tc);
    }

    if (tc->isNull())
        return;

    --*repeat;

    while (*repeat > 0) {
        if (!tc->hasSelection())
            tc->movePosition(Right);
        if (tc->atBlockEnd())
            tc->movePosition(NextBlock);
        *tc = doc->find(needleExp, *tc);
        if (tc->isNull())
            return;
        --*repeat;
    }

    if (!tc->isNull() && afterEndOfLine(doc, tc->anchor()))
        tc->movePosition(Left);
}

static void searchBackward(QTextCursor *tc, const QRegularExpression &needleExp, int *repeat)
{
    // Search from beginning of line so that matched text is the same.
    QTextBlock block = tc->block();
    QString line = block.text();

    QRegularExpressionMatch match;
    int i = line.indexOf(needleExp, 0, &match);
    while (i != -1 && i < tc->positionInBlock()) {
        --*repeat;
        const int offset = i + qMax(1, match.capturedLength());
        i = line.indexOf(needleExp, offset, &match);
        if (i == line.size())
            i = -1;
    }

    if (i == tc->positionInBlock())
        --*repeat;

    while (*repeat > 0) {
        block = block.previous();
        if (!block.isValid())
            break;
        line = block.text();
        i = line.indexOf(needleExp, 0, &match);
        while (i != -1) {
            --*repeat;
            const int offset = i + qMax(1, match.capturedLength());
            i = line.indexOf(needleExp, offset, &match);
            if (i == line.size())
                i = -1;
        }
    }

    if (!block.isValid()) {
        *tc = QTextCursor();
        return;
    }

    i = line.indexOf(needleExp, 0, &match);
    while (*repeat < 0) {
        const int offset = i + qMax(1, match.capturedLength());
        i = line.indexOf(needleExp, offset, &match);
        ++*repeat;
    }
    tc->setPosition(block.position() + i);
    tc->setPosition(tc->position() + match.capturedLength(), KeepAnchor);
}

// Commands [[, []
static void bracketSearchBackward(QTextCursor *tc, const QString &needleExp, int repeat)
{
    const QRegularExpression re(needleExp);
    QTextCursor tc2 = *tc;
    tc2.setPosition(tc2.position() - 1);
    searchBackward(&tc2, re, &repeat);
    if (repeat <= 1)
        tc->setPosition(tc2.isNull() ? 0 : tc2.position(), KeepAnchor);
}

// Commands ][, ]]
// When ]] is used after an operator, then also stops below a '}' in the first column.
static void bracketSearchForward(QTextCursor *tc, const QString &needleExp, int repeat,
                                 bool searchWithCommand)
{
    QRegularExpression re(searchWithCommand ? QString("^\\}|^\\{") : needleExp);
    QTextCursor tc2 = *tc;
    tc2.setPosition(tc2.position() + 1);
    searchForward(&tc2, re, &repeat);
    if (repeat <= 1) {
        if (tc2.isNull()) {
            tc->setPosition(tc->document()->characterCount() - 1, KeepAnchor);
        } else {
            tc->setPosition(tc2.position() - 1, KeepAnchor);
            if (searchWithCommand && tc->document()->characterAt(tc->position()).unicode() == '}') {
                QTextBlock block = tc->block().next();
                if (block.isValid())
                    tc->setPosition(block.position(), KeepAnchor);
            }
        }
    }
}

static char backslashed(char t)
{
    switch (t) {
        case 'e': return 27;
        case 't': return '\t';
        case 'r': return '\r';
        case 'n': return '\n';
        case 'b': return 8;
    }
    return t;
}

static bool substituteText(QString *text, QRegularExpression &pattern, const QString &replacement,
    bool global)
{
    bool substituted = false;
    int pos = 0;
    int right = -1;
    QRegularExpressionMatch m;
    while (true) {
        m = pattern.match(*text, pos);
        pos = m.capturedStart();
        if (pos == -1)
            break;

        // ensure that substitution is advancing towards end of line
        if (right == text->size() - pos) {
            ++pos;
            if (pos == text->size())
                break;
            continue;
        }

        right = text->size() - pos;

        substituted = true;
        QString matched = text->mid(pos, m.capturedLength());
        QString repl;
        bool escape = false;
        // insert captured texts
        for (int i = 0; i < replacement.size(); ++i) {
            const QChar &c = replacement[i];
            if (escape) {
                escape = false;
                if (c.isDigit()) {
                    if (c.digitValue() <= m.capturedTexts().size())
                        repl += m.captured(c.digitValue());
                } else {
                    repl += backslashed(c.unicode());
                }
            } else {
                if (c == '\\')
                    escape = true;
                else if (c == '&')
                    repl += m.captured(0);
                else
                    repl += c;
            }
        }
        text->replace(pos, matched.size(), repl);
        pos += (repl.isEmpty() && matched.isEmpty()) ? 1 : repl.size();

        if (pos >= text->size() || !global)
            break;
    }

    return substituted;
}

static int findUnescaped(QChar c, const QString &line, int from)
{
    for (int i = from; i < line.size(); ++i) {
        if (line.at(i) == c && (i == 0 || line.at(i - 1) != '\\'))
            return i;
    }
    return -1;
}

static void setClipboardData(const QString &content, RangeMode mode,
    QClipboard::Mode clipboardMode)
{
    QClipboard *clipboard = QApplication::clipboard();
    char vimRangeMode = mode;

    QByteArray bytes1;
    bytes1.append(vimRangeMode);
    bytes1.append(content.toUtf8());

    QByteArray bytes2;
    bytes2.append(vimRangeMode);
    bytes2.append("utf-8");
    bytes2.append('\0');
    bytes2.append(content.toUtf8());

    auto data = new QMimeData;
    data->setText(content);
    data->setData(vimMimeText, bytes1);
    data->setData(vimMimeTextEncoded, bytes2);
    clipboard->setMimeData(data, clipboardMode);
}

static QByteArray toLocalEncoding(const QString &text)
{
#if defined(Q_OS_WIN)
    return QString(text).replace("\n", "\r\n").toLocal8Bit();
#else
    return text.toLocal8Bit();
#endif
}

static QString fromLocalEncoding(const QByteArray &data)
{
#if defined(Q_OS_WIN)
    return QString::fromLocal8Bit(data).replace("\n", "\r\n");
#else
    return QString::fromLocal8Bit(data);
#endif
}

static QString getProcessOutput(const QString &command, const QString &input)
{
    QProcess proc;
    proc.start(command);
    proc.waitForStarted();
    proc.write(toLocalEncoding(input));
    proc.closeWriteChannel();

    // FIXME: Process should be interruptable by user.
    //        Solution is to create a QObject for each process and emit finished state.
    proc.waitForFinished();

    return fromLocalEncoding(proc.readAllStandardOutput());
}

static const QMap<QString, int> &vimKeyNames()
{
    static const QMap<QString, int> k = {
        // FIXME: Should be value of mapleader.
        {"LEADER", Key_Backslash},

        {"SPACE", Key_Space},
        {"TAB", Key_Tab},
        {"NL", Key_Return},
        {"NEWLINE", Key_Return},
        {"LINEFEED", Key_Return},
        {"LF", Key_Return},
        {"CR", Key_Return},
        {"RETURN", Key_Return},
        {"ENTER", Key_Return},
        {"BS", Key_Backspace},
        {"BACKSPACE", Key_Backspace},
        {"ESC", Key_Escape},
        {"BAR", Key_Bar},
        {"BSLASH", Key_Backslash},
        {"DEL", Key_Delete},
        {"DELETE", Key_Delete},
        {"KDEL", Key_Delete},
        {"UP", Key_Up},
        {"DOWN", Key_Down},
        {"LEFT", Key_Left},
        {"RIGHT", Key_Right},

        {"LT", Key_Less},
        {"GT", Key_Greater},

        {"F1", Key_F1},
        {"F2", Key_F2},
        {"F3", Key_F3},
        {"F4", Key_F4},
        {"F5", Key_F5},
        {"F6", Key_F6},
        {"F7", Key_F7},
        {"F8", Key_F8},
        {"F9", Key_F9},
        {"F10", Key_F10},

        {"F11", Key_F11},
        {"F12", Key_F12},
        {"F13", Key_F13},
        {"F14", Key_F14},
        {"F15", Key_F15},
        {"F16", Key_F16},
        {"F17", Key_F17},
        {"F18", Key_F18},
        {"F19", Key_F19},
        {"F20", Key_F20},

        {"F21", Key_F21},
        {"F22", Key_F22},
        {"F23", Key_F23},
        {"F24", Key_F24},
        {"F25", Key_F25},
        {"F26", Key_F26},
        {"F27", Key_F27},
        {"F28", Key_F28},
        {"F29", Key_F29},
        {"F30", Key_F30},

        {"F31", Key_F31},
        {"F32", Key_F32},
        {"F33", Key_F33},
        {"F34", Key_F34},
        {"F35", Key_F35},

        {"INSERT", Key_Insert},
        {"INS", Key_Insert},
        {"KINSERT", Key_Insert},
        {"HOME", Key_Home},
        {"END", Key_End},
        {"PAGEUP", Key_PageUp},
        {"PAGEDOWN", Key_PageDown},

        {"KPLUS", Key_Plus},
        {"KMINUS", Key_Minus},
        {"KDIVIDE", Key_Slash},
        {"KMULTIPLY", Key_Asterisk},
        {"KENTER", Key_Enter},
        {"KPOINT", Key_Period},

        {"CAPS", Key_CapsLock},
        {"NUM", Key_NumLock},
        {"SCROLL", Key_ScrollLock},
        {"ALTGR", Key_AltGr}
    };

    return k;
}

static bool isOnlyControlModifier(const Qt::KeyboardModifiers &mods)
{
    return (mods ^ ControlModifier) == Qt::NoModifier;
}


Range::Range(int b, int e, RangeMode m)
    : beginPos(qMin(b, e)), endPos(qMax(b, e)), rangemode(m)
{}

QString Range::toString() const
{
    return QString("%1-%2 (mode: %3)").arg(beginPos).arg(endPos).arg(rangemode);
}

bool Range::isValid() const
{
    return beginPos >= 0 && endPos >= 0;
}

QDebug operator<<(QDebug ts, const Range &range)
{
    return ts << '[' << range.beginPos << ',' << range.endPos << ']';
}


ExCommand::ExCommand(const QString &c, const QString &a, const Range &r)
    : cmd(c), args(a), range(r)
{}

bool ExCommand::matches(const QString &min, const QString &full) const
{
    return cmd.startsWith(min) && full.startsWith(cmd);
}

QDebug operator<<(QDebug ts, const ExCommand &cmd)
{
    return ts << cmd.cmd << ' ' << cmd.args << ' ' << cmd.range;
}

QDebug operator<<(QDebug ts, const QList<QTextEdit::ExtraSelection> &sels)
{
    foreach (const QTextEdit::ExtraSelection &sel, sels)
        ts << "SEL: " << sel.cursor.anchor() << sel.cursor.position();
    return ts;
}

QString quoteUnprintable(const QString &ba)
{
    QString res;
    for (int i = 0, n = ba.size(); i != n; ++i) {
        const QChar c = ba.at(i);
        const int cc = c.unicode();
        if (c.isPrint())
            res += c;
        else if (cc == '\n')
            res += "<CR>";
        else
            res += QString("\\x%1").arg(c.unicode(), 2, 16, QLatin1Char('0'));
    }
    return res;
}

static bool startsWithWhitespace(const QString &str, int col)
{
    if (col > str.size()) {
        qWarning("Wrong column");
        return false;
    }
    for (int i = 0; i < col; ++i) {
        uint u = str.at(i).unicode();
        if (u != ' ' && u != '\t')
            return false;
    }
    return true;
}

inline QString msgMarkNotSet(const QString &text)
{
    return Tr::tr("Mark \"%1\" not set.").arg(text);
}

static void initSingleShotTimer(QTimer *timer, int interval, FakeVimHandler::Private *receiver,
                                void (FakeVimHandler::Private::*slot)())
{
    timer->setSingleShot(true);
    timer->setInterval(interval);
    QObject::connect(timer, &QTimer::timeout, receiver, slot);
}

class Input final
{
public:
    // Remove some extra "information" on Mac.
    static Qt::KeyboardModifiers cleanModifier(Qt::KeyboardModifiers m)
    {
        return m & ~Qt::KeypadModifier;
    }

    Input() = default;
    explicit Input(QChar x)
        : m_key(x.unicode()), m_xkey(x.unicode()), m_text(x)
    {
        if (x.isUpper())
            m_modifiers = Qt::ShiftModifier;
        else if (x.isLower())
            m_key = x.toUpper().unicode();
    }

    Input(int k, Qt::KeyboardModifiers m, const QString &t = QString())
        : m_key(k), m_modifiers(cleanModifier(m)), m_text(t)
    {
        if (m_text.size() == 1) {
            QChar x = m_text.at(0);

            // On Mac, QKeyEvent::text() returns non-empty strings for
            // cursor keys. This breaks some of the logic later on
            // relying on text() being empty for "special" keys.
            // FIXME: Check the real conditions.
            if (x.unicode() < ' ' && x.unicode() != 27)
                m_text.clear();
            else if (x.isLetter())
                m_key = x.toUpper().unicode();
        }

        // Set text only if input is ascii key without control modifier.
        if (m_text.isEmpty() && k >= 0 && k <= 0x7f && (m & ControlModifier) == 0) {
            QChar c = QChar(k);
            if (c.isLetter())
                m_text = isShift() ? c.toUpper() : c;
            else if (!isShift())
                m_text = c;
        }

        // Normalize <S-TAB>.
        if (m_key == Qt::Key_Backtab) {
            m_key = Qt::Key_Tab;
            m_modifiers |= Qt::ShiftModifier;
        }

        // m_xkey is only a cache.
        m_xkey = (m_text.size() == 1 ? m_text.at(0).unicode() : m_key);
    }

    bool isValid() const
    {
        return m_key != 0 || !m_text.isNull();
    }

    bool isDigit() const
    {
        return m_xkey >= '0' && m_xkey <= '9';
    }

    bool isKey(int c) const
    {
        return !m_modifiers && m_key == c;
    }

    bool isBackspace() const
    {
        return m_key == Key_Backspace || isControl('h');
    }

    bool isReturn() const
    {
        return m_key == '\n' || m_key == Key_Return || m_key == Key_Enter;
    }

    bool isEscape() const
    {
        return isKey(Key_Escape) || isShift(Key_Escape) || isKey(27) || isShift(27) || isControl('c')
            || isControl(Key_BracketLeft);
    }

    bool is(int c) const
    {
        return m_xkey == c && !isControl();
    }

    bool isControl() const
    {
        return isOnlyControlModifier(m_modifiers);
    }

    bool isControl(int c) const
    {
        return isControl()
            && (m_xkey == c || m_xkey + 32 == c || m_xkey + 64 == c || m_xkey + 96 == c);
    }

    bool isShift() const
    {
        return m_modifiers & Qt::ShiftModifier;
    }

    bool isShift(int c) const
    {
        return isShift() && m_xkey == c;
    }

    bool operator<(const Input &a) const
    {
        if (m_key != a.m_key)
            return m_key < a.m_key;
        // Text for some mapped key cannot be determined (e.g. <C-J>) so if text is not set for
        // one of compared keys ignore it.
        if (!m_text.isEmpty() && !a.m_text.isEmpty() && m_text != " ")
            return m_text < a.m_text;
        return m_modifiers < a.m_modifiers;
    }

    bool operator==(const Input &a) const
    {
        return !(*this < a || a < *this);
    }

    bool operator!=(const Input &a) const { return !operator==(a); }

    QString text() const { return m_text; }

    QChar asChar() const
    {
        return (m_text.size() == 1 ? m_text.at(0) : QChar());
    }

    int toInt(bool *ok, int base) const
    {
        const int uc = asChar().unicode();
        int res;
        if ('0' <= uc && uc <= '9')
            res = uc -'0';
        else if ('a' <= uc && uc <= 'z')
            res = 10 + uc - 'a';
        else if ('A' <= uc && uc <= 'Z')
            res = 10 + uc - 'A';
        else
            res = base;
        *ok = res < base;
        return *ok ? res : 0;
    }

    int key() const { return m_key; }

    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }

    // Return raw character for macro recording or dot command.
    QChar raw() const
    {
        if (m_key == Key_Tab)
            return '\t';
        if (m_key == Key_Return)
            return '\n';
        if (m_key == Key_Escape)
            return QChar(27);
        return m_xkey;
    }

    QString toString() const
    {
        if (!m_text.isEmpty())
            return QString(m_text).replace("<", "<LT>");

        QString key = vimKeyNames().key(m_key);
        bool namedKey = !key.isEmpty();

        if (!namedKey) {
            if (m_xkey == '<')
                key = "<LT>";
            else if (m_xkey == '>')
                key = "<GT>";
            else
                key = QChar(m_xkey);
        }

        bool shift = isShift();
        bool ctrl = isControl();
        if (shift)
            key.prepend("S-");
        if (ctrl)
            key.prepend("C-");

        if (namedKey || shift || ctrl) {
            key.prepend('<');
            key.append('>');
        }

        return key;
    }

    QDebug dump(QDebug ts) const
    {
        return ts << m_key << '-' << m_modifiers << '-'
            << quoteUnprintable(m_text);
    }

private:
    int m_key = 0;
    int m_xkey = 0;
    Qt::KeyboardModifiers m_modifiers = NoModifier;
    QString m_text;
};

// mapping to <Nop> (do nothing)
static const Input Nop(-1, Qt::KeyboardModifiers(-1), QString());

static SubMode letterCaseModeFromInput(const Input &input)
{
    if (input.is('~'))
        return InvertCaseSubMode;
    if (input.is('u'))
        return DownCaseSubMode;
    if (input.is('U'))
        return UpCaseSubMode;

    return NoSubMode;
}

static SubMode indentModeFromInput(const Input &input)
{
    if (input.is('<'))
        return ShiftLeftSubMode;
    if (input.is('>'))
        return ShiftRightSubMode;
    if (input.is('='))
        return IndentSubMode;

    return NoSubMode;
}

static SubMode changeDeleteYankModeFromInput(const Input &input)
{
    if (input.is('c'))
        return ChangeSubMode;
    if (input.is('d'))
        return DeleteSubMode;
    if (input.is('y'))
        return YankSubMode;

    return NoSubMode;
}

QString dotCommandFromSubMode(SubMode submode)
{
    if (submode == ChangeSubMode)
        return QLatin1String("c");
    if (submode == DeleteSubMode)
        return QLatin1String("d");
    if (submode == InvertCaseSubMode)
        return QLatin1String("g~");
    if (submode == DownCaseSubMode)
        return QLatin1String("gu");
    if (submode == UpCaseSubMode)
        return QLatin1String("gU");
    if (submode == IndentSubMode)
        return QLatin1String("=");
    if (submode == ShiftRightSubMode)
        return QLatin1String(">");
    if (submode == ShiftLeftSubMode)
        return QLatin1String("<");

    return QString();
}

QDebug operator<<(QDebug ts, const Input &input) { return input.dump(ts); }

class Inputs final : public QVector<Input>
{
public:
    Inputs() = default;

    explicit Inputs(const QString &str, bool noremap = true, bool silent = false)
        : m_noremap(noremap), m_silent(silent)
    {
        parseFrom(str);
        squeeze();
    }

    bool noremap() const { return m_noremap; }

    bool silent() const { return m_silent; }

private:
    void parseFrom(const QString &str);

    bool m_noremap = true;
    bool m_silent = false;
};

static Input parseVimKeyName(const QString &keyName)
{
    if (keyName.length() == 1)
        return Input(keyName.at(0));

    const QStringList keys = keyName.split('-');
    const int len = keys.length();

    if (len == 1 && keys.at(0).toUpper() == "NOP")
        return Nop;

    Qt::KeyboardModifiers mods = NoModifier;
    for (int i = 0; i < len - 1; ++i) {
        const QString &key = keys[i].toUpper();
        if (key == "S")
            mods |= Qt::ShiftModifier;
        else if (key == "C")
            mods |= ControlModifier;
        else
            return Input();
    }

    if (!keys.isEmpty()) {
        const QString key = keys.last();
        if (key.length() == 1) {
            // simple character
            QChar c = key.at(0).toUpper();
            return Input(c.unicode(), mods);
        }

        // find key name
        QMap<QString, int>::ConstIterator it = vimKeyNames().constFind(key.toUpper());
        if (it != vimKeyNames().end())
            return Input(*it, mods);
    }

    return Input();
}

void Inputs::parseFrom(const QString &str)
{
    const int n = str.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = str.at(i);
        if (c == '<') {
            int j = str.indexOf('>', i);
            Input input;
            if (j != -1) {
                const QString key = str.mid(i+1, j - i - 1);
                if (!key.contains('<'))
                    input = parseVimKeyName(key);
            }
            if (input.isValid()) {
                append(input);
                i = j;
            } else {
                append(Input(c));
            }
        } else {
            append(Input(c));
        }
    }
}

class History final
{
public:
    History() : m_items(QString()) {}
    void append(const QString &item);
    const QString &move(const QStringRef &prefix, int skip);
    const QString &current() const { return m_items[m_index]; }
    const QStringList &items() const { return m_items; }
    void restart() { m_index = m_items.size() - 1; }

private:
    // Last item is always empty or current search prefix.
    QStringList m_items;
    int m_index = 0;
};

void History::append(const QString &item)
{
    if (item.isEmpty())
        return;
    m_items.pop_back();
    m_items.removeAll(item);
    m_items << item << QString();
    restart();
}

const QString &History::move(const QStringRef &prefix, int skip)
{
    if (!current().startsWith(prefix))
        restart();

    if (m_items.last() != prefix)
        m_items[m_items.size() - 1] = prefix.toString();

    int i = m_index + skip;
    if (!prefix.isEmpty())
        for (; i >= 0 && i < m_items.size() && !m_items[i].startsWith(prefix); i += skip)
            ;
    if (i >= 0 && i < m_items.size())
        m_index = i;

    return current();
}

// Command line buffer with prompt (i.e. :, / or ? characters), text contents and cursor position.
class CommandBuffer final
{
public:
    void setPrompt(const QChar &prompt) { m_prompt = prompt; }
    void setContents(const QString &s) { m_buffer = s; m_anchor = m_pos = s.size(); }

    void setContents(const QString &s, int pos, int anchor = -1)
    {
        m_buffer = s; m_pos = m_userPos = pos; m_anchor = anchor >= 0 ? anchor : pos;
    }

    QStringRef userContents() const { return m_buffer.leftRef(m_userPos); }
    const QChar &prompt() const { return m_prompt; }
    const QString &contents() const { return m_buffer; }
    bool isEmpty() const { return m_buffer.isEmpty(); }
    int cursorPos() const { return m_pos; }
    int anchorPos() const { return m_anchor; }
    bool hasSelection() const { return m_pos != m_anchor; }

    void insertChar(QChar c) { m_buffer.insert(m_pos++, c); m_anchor = m_userPos = m_pos; }
    void insertText(const QString &s)
    {
        m_buffer.insert(m_pos, s); m_anchor = m_userPos = m_pos = m_pos + s.size();
    }
    void deleteChar() { if (m_pos) m_buffer.remove(--m_pos, 1); m_anchor = m_userPos = m_pos; }

    void moveLeft() { if (m_pos) m_userPos = --m_pos; }
    void moveRight() { if (m_pos < m_buffer.size()) m_userPos = ++m_pos; }
    void moveStart() { m_userPos = m_pos = 0; }
    void moveEnd() { m_userPos = m_pos = m_buffer.size(); }

    void setHistoryAutoSave(bool autoSave) { m_historyAutoSave = autoSave; }
    void historyDown() { setContents(m_history.move(userContents(), 1)); }
    void historyUp() { setContents(m_history.move(userContents(), -1)); }
    const QStringList &historyItems() const { return m_history.items(); }
    void historyPush(const QString &item = QString())
    {
        m_history.append(item.isNull() ? contents() : item);
    }

    void clear()
    {
        if (m_historyAutoSave)
            historyPush();
        m_buffer.clear();
        m_anchor = m_userPos = m_pos = 0;
    }

    QString display() const
    {
        QString msg(m_prompt);
        for (int i = 0; i != m_buffer.size(); ++i) {
            const QChar c = m_buffer.at(i);
            if (c.unicode() < 32) {
                msg += '^';
                msg += QChar(c.unicode() + 64);
            } else {
                msg += c;
            }
        }
        return msg;
    }

    void deleteSelected()
    {
        if (m_pos < m_anchor) {
            m_buffer.remove(m_pos, m_anchor - m_pos);
            m_anchor = m_pos;
        } else {
            m_buffer.remove(m_anchor, m_pos - m_anchor);
            m_pos = m_anchor;
        }
    }

    bool handleInput(const Input &input)
    {
        if (input.isShift(Key_Left)) {
            moveLeft();
        } else if (input.isShift(Key_Right)) {
            moveRight();
        } else if (input.isShift(Key_Home)) {
            moveStart();
        } else if (input.isShift(Key_End)) {
            moveEnd();
        } else if (input.isKey(Key_Left)) {
            moveLeft();
            m_anchor = m_pos;
        } else if (input.isKey(Key_Right)) {
            moveRight();
            m_anchor = m_pos;
        } else if (input.isKey(Key_Home)) {
            moveStart();
            m_anchor = m_pos;
        } else if (input.isKey(Key_End)) {
            moveEnd();
            m_anchor = m_pos;
        } else if (input.isKey(Key_Up) || input.isKey(Key_PageUp)) {
            historyUp();
        } else if (input.isKey(Key_Down) || input.isKey(Key_PageDown)) {
            historyDown();
        } else if (input.isKey(Key_Delete)) {
            if (hasSelection()) {
                deleteSelected();
            } else {
                if (m_pos < m_buffer.size())
                    m_buffer.remove(m_pos, 1);
                else
                    deleteChar();
            }
        } else if (!input.text().isEmpty()) {
            if (hasSelection())
                deleteSelected();
            insertText(input.text());
        } else {
            return false;
        }
        return true;
    }

private:
    QString m_buffer;
    QChar m_prompt;
    History m_history;
    int m_pos = 0;
    int m_anchor = 0;
    int m_userPos = 0; // last position of inserted text (for retrieving history items)
    bool m_historyAutoSave = true; // store items to history on clear()?
};

// Mappings for a specific mode (trie structure)
class ModeMapping final : public QMap<Input, ModeMapping>
{
public:
    const Inputs &value() const { return m_value; }
    void setValue(const Inputs &value) { m_value = value; }
private:
    Inputs m_value;
};

// Mappings for all modes
using Mappings = QHash<char, ModeMapping>;

// Iterator for mappings
class MappingsIterator final : public QVector<ModeMapping::Iterator>
{
public:
    MappingsIterator(Mappings *mappings, char mode = -1, const Inputs &inputs = Inputs())
        : m_parent(mappings)
    {
        reset(mode);
        walk(inputs);
    }

    // Reset iterator state. Keep previous mode if 0.
    void reset(char mode = 0)
    {
        clear();
        m_lastValid = -1;
        m_currentInputs.clear();
        if (mode != 0) {
            m_mode = mode;
            if (mode != -1)
                m_modeMapping = m_parent->find(mode);
        }
    }

    bool isValid() const { return !empty(); }

    // Return true if mapping can be extended.
    bool canExtend() const { return isValid() && !last()->empty(); }

    // Return true if this mapping can be used.
    bool isComplete() const { return m_lastValid != -1; }

    // Return size of current map.
    int mapLength() const { return m_lastValid + 1; }

    bool walk(const Input &input)
    {
        m_currentInputs.append(input);

        if (m_modeMapping == m_parent->end())
            return false;

        ModeMapping::Iterator it;
        if (isValid()) {
            it = last()->find(input);
            if (it == last()->end())
                return false;
        } else {
            it = m_modeMapping->find(input);
            if (it == m_modeMapping->end())
                return false;
        }

        if (!it->value().isEmpty())
            m_lastValid = size();
        append(it);

        return true;
    }

    bool walk(const Inputs &inputs)
    {
        foreach (const Input &input, inputs) {
            if (!walk(input))
                return false;
        }
        return true;
    }

    // Return current mapped value. Iterator must be valid.
    const Inputs &inputs() const
    {
        return at(m_lastValid)->value();
    }

    void remove()
    {
        if (isValid()) {
            if (canExtend()) {
                last()->setValue(Inputs());
            } else {
                if (size() > 1) {
                    while (last()->empty()) {
                        at(size() - 2)->erase(last());
                        pop_back();
                        if (size() == 1 || !last()->value().isEmpty())
                            break;
                    }
                    if (last()->empty() && last()->value().isEmpty())
                        m_modeMapping->erase(last());
                } else if (last()->empty() && !last()->value().isEmpty()) {
                    m_modeMapping->erase(last());
                }
            }
        }
    }

    void setInputs(const Inputs &key, const Inputs &inputs, bool unique = false)
    {
        ModeMapping *current = &(*m_parent)[m_mode];
        foreach (const Input &input, key)
            current = &(*current)[input];
        if (!unique || current->value().isEmpty())
            current->setValue(inputs);
    }

    const Inputs &currentInputs() const { return m_currentInputs; }

private:
    Mappings *m_parent;
    Mappings::Iterator m_modeMapping;
    int m_lastValid = -1;
    char m_mode = 0;
    Inputs m_currentInputs;
};

// state of current mapping
struct MappingState {
    MappingState() = default;
    MappingState(bool noremap, bool silent, bool editBlock)
        : noremap(noremap), silent(silent), editBlock(editBlock) {}
    bool noremap = false;
    bool silent = false;
    bool editBlock = false;
};

class FakeVimHandler::Private : public QObject
{
public:
    Private(FakeVimHandler *parent, QWidget *widget);

    EventResult handleEvent(QKeyEvent *ev);
    bool wantsOverride(QKeyEvent *ev);
    bool parseExCommand(QString *line, ExCommand *cmd);
    bool parseLineRange(QString *line, ExCommand *cmd);
    int parseLineAddress(QString *cmd);
    void parseRangeCount(const QString &line, Range *range) const;
    void handleCommand(const QString &cmd); // Sets m_tc + handleExCommand
    void handleExCommand(const QString &cmd);

    void installEventFilter();
    void removeEventFilter();
    void passShortcuts(bool enable);
    void setupWidget();
    void restoreWidget(int tabSize);

    friend class FakeVimHandler;

    void init();
    void focus();
    void unfocus();
    void fixExternalCursor(bool focus);
    void fixExternalCursorPosition(bool focus);

    // Call before any FakeVim processing (import cursor position from editor)
    void enterFakeVim();
    // Call after any FakeVim processing
    // (if needUpdate is true, export cursor position to editor and scroll)
    void leaveFakeVim(bool needUpdate = true);
    void leaveFakeVim(EventResult eventResult);

    EventResult handleKey(const Input &input);
    EventResult handleDefaultKey(const Input &input);
    bool handleCommandBufferPaste(const Input &input);
    EventResult handleCurrentMapAsDefault();
    void prependInputs(const QVector<Input> &inputs); // Handle inputs.
    void prependMapping(const Inputs &inputs); // Handle inputs as mapping.
    bool expandCompleteMapping(); // Return false if current mapping is not complete.
    bool extendMapping(const Input &input); // Return false if no suitable mappig found.
    void endMapping();
    bool canHandleMapping();
    void clearPendingInput();
    void waitForMapping();
    EventResult stopWaitForMapping(bool hasInput);
    EventResult handleInsertOrReplaceMode(const Input &);
    void handleInsertMode(const Input &);
    void handleReplaceMode(const Input &);
    void finishInsertMode();

    EventResult handleCommandMode(const Input &);

    // return true only if input in current mode and sub-mode was correctly handled
    bool handleEscape();
    bool handleNoSubMode(const Input &);
    bool handleChangeDeleteYankSubModes(const Input &);
    void handleChangeDeleteYankSubModes();
    bool handleReplaceSubMode(const Input &);
    bool handleFilterSubMode(const Input &);
    bool handleRegisterSubMode(const Input &);
    bool handleShiftSubMode(const Input &);
    bool handleChangeCaseSubMode(const Input &);
    bool handleWindowSubMode(const Input &);
    bool handleZSubMode(const Input &);
    bool handleCapitalZSubMode(const Input &);
    bool handleMacroRecordSubMode(const Input &);
    bool handleMacroExecuteSubMode(const Input &);

    bool handleCount(const Input &); // Handle count for commands (return false if input isn't count).
    bool handleMovement(const Input &);

    EventResult handleExMode(const Input &);
    EventResult handleSearchSubSubMode(const Input &);
    bool handleCommandSubSubMode(const Input &);
    void fixSelection(); // Fix selection according to current range, move and command modes.
    bool finishSearch();
    void finishMovement(const QString &dotCommandMovement = QString());

    // Returns to insert/replace mode after <C-O> command in insert mode,
    // otherwise returns to command mode.
    void leaveCurrentMode();

    // Clear data for current (possibly incomplete) command in current mode.
    // I.e. clears count, register, g flag, sub-modes etc.
    void clearCurrentMode();

    QTextCursor search(const SearchData &sd, int startPos, int count, bool showMessages);
    void search(const SearchData &sd, bool showMessages = true);
    bool searchNext(bool forward = true);
    void searchBalanced(bool forward, QChar needle, QChar other);
    void highlightMatches(const QString &needle);
    void stopIncrementalFind();
    void updateFind(bool isComplete);

    void resetCount();
    bool isInputCount(const Input &) const; // Return true if input can be used as count for commands.
    int mvCount() const { return qMax(1, g.mvcount); }
    int opCount() const { return qMax(1, g.opcount); }
    int count() const { return mvCount() * opCount(); }
    QTextBlock block() const { return m_cursor.block(); }
    int leftDist() const { return position() - block().position(); }
    int rightDist() const { return block().length() - leftDist() - (isVisualCharMode() ? 0 : 1); }
    bool atBlockStart() const { return m_cursor.atBlockStart(); }
    bool atBlockEnd() const { return m_cursor.atBlockEnd(); }
    bool atEndOfLine() const { return atBlockEnd() && block().length() > 1; }
    bool atDocumentEnd() const { return position() >= lastPositionInDocument(true); }
    bool atDocumentStart() const { return m_cursor.atStart(); }

    bool atEmptyLine(int pos) const;
    bool atEmptyLine(const QTextCursor &tc) const;
    bool atEmptyLine() const;
    bool atBoundary(bool end, bool simple, bool onlyWords = false,
        const QTextCursor &tc = QTextCursor()) const;
    bool atWordBoundary(bool end, bool simple, const QTextCursor &tc = QTextCursor()) const;
    bool atWordStart(bool simple, const QTextCursor &tc = QTextCursor()) const;
    bool atWordEnd(bool simple, const QTextCursor &tc = QTextCursor()) const;
    bool isFirstNonBlankOnLine(int pos);

    int lastPositionInDocument(bool ignoreMode = false) const; // Returns last valid position in doc.
    int firstPositionInLine(int line, bool onlyVisibleLines = true) const; // 1 based line, 0 based pos
    int lastPositionInLine(int line, bool onlyVisibleLines = true) const; // 1 based line, 0 based pos
    int lineForPosition(int pos) const;  // 1 based line, 0 based pos
    QString lineContents(int line) const; // 1 based line
    QString textAt(int from, int to) const;
    void setLineContents(int line, const QString &contents); // 1 based line
    int blockBoundary(const QString &left, const QString &right,
        bool end, int count) const; // end or start position of current code block
    int lineNumber(const QTextBlock &block) const;

    int columnAt(int pos) const;
    int blockNumberAt(int pos) const;
    QTextBlock blockAt(int pos) const;
    QTextBlock nextLine(const QTextBlock &block) const; // following line (respects wrapped parts)
    QTextBlock previousLine(const QTextBlock &block) const; // previous line (respects wrapped parts)

    int linesOnScreen() const;
    int linesInDocument() const;

    // The following use all zero-based counting.
    int cursorLineOnScreen() const;
    int cursorLine() const;
    int cursorBlockNumber() const; // "." address
    int physicalCursorColumn() const; // as stored in the data
    int logicalCursorColumn() const; // as visible on screen
    int physicalToLogicalColumn(int physical, const QString &text) const;
    int logicalToPhysicalColumn(int logical, const QString &text) const;
    int windowScrollOffset() const; // return scrolloffset but max half the current window height
    Column cursorColumn() const; // as visible on screen
    void updateFirstVisibleLine();
    int firstVisibleLine() const;
    int lastVisibleLine() const;
    int lineOnTop(int count = 1) const; // [count]-th line from top reachable without scrolling
    int lineOnBottom(int count = 1) const; // [count]-th line from bottom reachable without scrolling
    void scrollToLine(int line);
    void scrollUp(int count);
    void scrollDown(int count) { scrollUp(-count); }
    void updateScrollOffset();
    void alignViewportToCursor(Qt::AlignmentFlag align, int line = -1,
        bool moveToNonBlank = false);

    int lineToBlockNumber(int line) const;

    void setCursorPosition(const CursorPosition &p);
    void setCursorPosition(QTextCursor *tc, const CursorPosition &p);

    // Helper functions for indenting/
    bool isElectricCharacter(QChar c) const;
    void indentSelectedText(QChar lastTyped = QChar());
    void indentText(const Range &range, QChar lastTyped = QChar());
    void shiftRegionLeft(int repeat = 1);
    void shiftRegionRight(int repeat = 1);

    void moveToFirstNonBlankOnLine();
    void moveToFirstNonBlankOnLine(QTextCursor *tc);
    void moveToFirstNonBlankOnLineVisually();
    void moveToNonBlankOnLine(QTextCursor *tc);
    void moveToTargetColumn();
    void setTargetColumn();
    void moveToMatchingParanthesis();
    void moveToBoundary(bool simple, bool forward = true);
    void moveToNextBoundary(bool end, int count, bool simple, bool forward);
    void moveToNextBoundaryStart(int count, bool simple, bool forward = true);
    void moveToNextBoundaryEnd(int count, bool simple, bool forward = true);
    void moveToBoundaryStart(int count, bool simple, bool forward = true);
    void moveToBoundaryEnd(int count, bool simple, bool forward = true);
    void moveToNextWord(bool end, int count, bool simple, bool forward, bool emptyLines);
    void moveToNextWordStart(int count, bool simple, bool forward = true, bool emptyLines = true);
    void moveToNextWordEnd(int count, bool simple, bool forward = true, bool emptyLines = true);
    void moveToWordStart(int count, bool simple, bool forward = true, bool emptyLines = true);
    void moveToWordEnd(int count, bool simple, bool forward = true, bool emptyLines = true);

    // Convenience wrappers to reduce line noise.
    void moveToStartOfLine();
    void moveToStartOfLineVisually();
    void moveToEndOfLine();
    void moveToEndOfLineVisually();
    void moveToEndOfLineVisually(QTextCursor *tc);
    void moveBehindEndOfLine();
    void moveUp(int n = 1) { moveDown(-n); }
    void moveDown(int n = 1);
    void moveUpVisually(int n = 1) { moveDownVisually(-n); }
    void moveDownVisually(int n = 1);
    void moveVertically(int n = 1) {
        if (g.gflag) {
            g.movetype = MoveExclusive;
            moveDownVisually(n);
        } else {
            g.movetype = MoveLineWise;
            moveDown(n);
        }
    }
    void movePageDown(int count = 1);
    void movePageUp(int count = 1) { movePageDown(-count); }
    void dump(const char *msg) const {
        qDebug() << msg << "POS: " << anchor() << position()
            << "VISUAL: " << g.visualMode;
    }
    void moveRight(int n = 1) {
        if (isVisualCharMode()) {
            const QTextBlock currentBlock = block();
            const int max = currentBlock.position() + currentBlock.length() - 1;
            const int pos = position() + n;
            setPosition(qMin(pos, max));
        } else {
            m_cursor.movePosition(Right, KeepAnchor, n);
        }
        if (atEndOfLine())
            q->fold(1, false);
        setTargetColumn();
    }
    void moveLeft(int n = 1) {
        m_cursor.movePosition(Left, KeepAnchor, n);
        setTargetColumn();
    }
    void moveToNextCharacter() {
        moveRight();
        if (atEndOfLine())
            moveRight();
    }
    void moveToPreviousCharacter() {
        moveLeft();
        if (atBlockStart())
            moveLeft();
    }
    void setAnchor() {
        m_cursor.setPosition(position(), MoveAnchor);
    }
    void setAnchor(int position) {
        m_cursor.setPosition(position, KeepAnchor);
    }
    void setPosition(int position) {
        m_cursor.setPosition(position, KeepAnchor);
    }
    void setAnchorAndPosition(int anchor, int position) {
        m_cursor.setPosition(anchor, MoveAnchor);
        m_cursor.setPosition(position, KeepAnchor);
    }

    // Set cursor in text editor widget.
    void commitCursor();

    // Restore cursor from editor widget.
    // Update selection, record jump and target column if cursor position
    // changes externally (e.g. by code completion).
    void pullCursor();

    QTextCursor editorCursor() const;

    // Values to save when starting FakeVim processing.
    int m_firstVisibleLine;
    QTextCursor m_cursor;
    bool m_cursorNeedsUpdate;

    bool moveToPreviousParagraph(int count = 1) { return moveToNextParagraph(-count); }
    bool moveToNextParagraph(int count = 1);
    void moveToParagraphStartOrEnd(int direction = 1);

    bool handleFfTt(const QString &key, bool repeats = false);

    void enterVisualInsertMode(QChar command);
    void enterReplaceMode();
    void enterInsertMode();
    void enterInsertOrReplaceMode(Mode mode);
    void enterCommandMode(Mode returnToMode = CommandMode);
    void enterExMode(const QString &contents = QString());
    void showMessage(MessageLevel level, const QString &msg);
    void clearMessage() { showMessage(MessageInfo, QString()); }
    void notImplementedYet();
    void updateMiniBuffer();
    void updateSelection();
    void updateHighlights();
    void updateCursorShape();
    void setThinCursor(bool enable = true);
    bool hasThinCursor() const;
    QWidget *editor() const;
    QTextDocument *document() const { return EDITOR(document()); }
    QChar characterAt(int pos) const { return document()->characterAt(pos); }
    QChar characterAtCursor() const { return characterAt(position()); }

    void joinPreviousEditBlock();
    void beginEditBlock(bool largeEditBlock = false);
    void beginLargeEditBlock() { beginEditBlock(true); }
    void endEditBlock();
    void breakEditBlock() { m_buffer->breakEditBlock = true; }

    bool canModifyBufferData() const { return m_buffer->currentHandler.data() == this; }

    void onContentsChanged(int position, int charsRemoved, int charsAdded);
    void onCursorPositionChanged();
    void onUndoCommandAdded();

    void onInputTimeout();
    void onFixCursorTimeout();

    bool isCommandLineMode() const { return g.mode == ExMode || g.subsubmode == SearchSubSubMode; }
    bool isInsertMode() const { return g.mode == InsertMode || g.mode == ReplaceMode; }
    // Waiting for movement operator.
    bool isOperatorPending() const {
        return g.submode == ChangeSubMode
            || g.submode == DeleteSubMode
            || g.submode == FilterSubMode
            || g.submode == IndentSubMode
            || g.submode == ShiftLeftSubMode
            || g.submode == ShiftRightSubMode
            || g.submode == InvertCaseSubMode
            || g.submode == DownCaseSubMode
            || g.submode == UpCaseSubMode
            || g.submode == YankSubMode; }

    bool isVisualMode() const { return g.visualMode != NoVisualMode; }
    bool isNoVisualMode() const { return g.visualMode == NoVisualMode; }
    bool isVisualCharMode() const { return g.visualMode == VisualCharMode; }
    bool isVisualLineMode() const { return g.visualMode == VisualLineMode; }
    bool isVisualBlockMode() const { return g.visualMode == VisualBlockMode; }
    char currentModeCode() const;
    void updateEditor();

    void selectTextObject(bool simple, bool inner);
    void selectWordTextObject(bool inner);
    void selectWORDTextObject(bool inner);
    void selectSentenceTextObject(bool inner);
    void selectParagraphTextObject(bool inner);
    bool changeNumberTextObject(int count);
    // return true only if cursor is in a block delimited with correct characters
    bool selectBlockTextObject(bool inner, QChar left, QChar right);
    bool selectQuotedStringTextObject(bool inner, const QString &quote);

    void commitInsertState();
    void invalidateInsertState();
    bool isInsertStateValid() const;
    void clearLastInsertion();
    void ensureCursorVisible();
    void insertInInsertMode(const QString &text);

    // Macro recording
    bool startRecording(const Input &input);
    void record(const Input &input);
    void stopRecording();
    bool executeRegister(int reg);

    // Handle current command as synonym
    void handleAs(const QString &command);

public:
    QTextEdit *m_textedit;
    QPlainTextEdit *m_plaintextedit;
    bool m_wasReadOnly; // saves read-only state of document

    bool m_inFakeVim; // true if currently processing a key press or a command

    FakeVimHandler *q;
    int m_register;
    BlockInsertMode m_visualBlockInsert;

    bool m_anchorPastEnd;
    bool m_positionPastEnd; // '$' & 'l' in visual mode can move past eol

    QString m_currentFileName;

    int m_findStartPosition;

    int anchor() const { return m_cursor.anchor(); }
    int position() const { return m_cursor.position(); }

    // Transform text selected by cursor in current visual mode.
    using Transformation = std::function<QString(const QString &)>;
    void transformText(const Range &range, QTextCursor &tc, const std::function<void()> &transform) const;
    void transformText(const Range &range, const Transformation &transform);

    void insertText(QTextCursor &tc, const QString &text);
    void insertText(const Register &reg);
    void removeText(const Range &range);

    void invertCase(const Range &range);

    void upCase(const Range &range);

    void downCase(const Range &range);

    void replaceText(const Range &range, const QString &str);

    QString selectText(const Range &range) const;
    void setCurrentRange(const Range &range);
    Range currentRange() const { return Range(position(), anchor(), g.rangemode); }

    void yankText(const Range &range, int toregister);

    void pasteText(bool afterCursor);

    void cutSelectedText(int reg = 0);

    void joinLines(int count, bool preserveSpace = false);

    void insertNewLine();

    bool handleInsertInEditor(const Input &input);
    bool passEventToEditor(QEvent &event, QTextCursor &tc); // Pass event to editor widget without filtering. Returns true if event was processed.

    // undo handling
    int revision() const { return document()->availableUndoSteps(); }
    void undoRedo(bool undo);
    void undo();
    void redo();
    void pushUndoState(bool overwrite = true);

    // extra data for '.'
    void replay(const QString &text, int repeat = 1);
    void setDotCommand(const QString &cmd) { g.dotCommand = cmd; }
    void setDotCommand(const QString &cmd, int n) { g.dotCommand = cmd.arg(n); }
    QString visualDotCommand() const;

    // visual modes
    void toggleVisualMode(VisualMode visualMode);
    void leaveVisualMode();
    void saveLastVisualMode();

    // marks
    Mark mark(QChar code) const;
    void setMark(QChar code, CursorPosition position);
    // jump to valid mark return true if mark is valid and local
    bool jumpToMark(QChar mark, bool backTickMode);
    // update marks on undo/redo
    void updateMarks(const Marks &newMarks);
    CursorPosition markLessPosition() const { return mark('<').position(document()); }
    CursorPosition markGreaterPosition() const { return mark('>').position(document()); }

    // vi style configuration
    QVariant config(int code) const { return theFakeVimSetting(code)->value(); }
    bool hasConfig(int code) const { return config(code).toBool(); }
    bool hasConfig(int code, const QString &value) const
        { return config(code).toString().contains(value); }

    int m_targetColumn; // -1 if past end of line
    int m_visualTargetColumn; // 'l' can move past eol in visual mode only
    int m_targetColumnWrapped; // column in current part of wrapped line

    // auto-indent
    QString tabExpand(int len) const;
    Column indentation(const QString &line) const;
    void insertAutomaticIndentation(bool goingDown, bool forceAutoIndent = false);
    // number of autoindented characters
    void handleStartOfLine();

    // register handling
    QString registerContents(int reg) const;
    void setRegister(int reg, const QString &contents, RangeMode mode);
    RangeMode registerRangeMode(int reg) const;
    void getRegisterType(int *reg, bool *isClipboard, bool *isSelection, bool *append = nullptr) const;

    void recordJump(int position = -1);
    void jump(int distance);

    QList<QTextEdit::ExtraSelection> m_extraSelections;
    QTextCursor m_searchCursor;
    int m_searchStartPosition;
    int m_searchFromScreenLine;
    QString m_highlighted; // currently highlighted text

    bool handleExCommandHelper(ExCommand &cmd); // Returns success.
    bool handleExPluginCommand(const ExCommand &cmd); // Handled by plugin?
    bool handleExBangCommand(const ExCommand &cmd);
    bool handleExYankDeleteCommand(const ExCommand &cmd);
    bool handleExChangeCommand(const ExCommand &cmd);
    bool handleExMoveCommand(const ExCommand &cmd);
    bool handleExJoinCommand(const ExCommand &cmd);
    bool handleExGotoCommand(const ExCommand &cmd);
    bool handleExHistoryCommand(const ExCommand &cmd);
    bool handleExRegisterCommand(const ExCommand &cmd);
    bool handleExMapCommand(const ExCommand &cmd);
    bool handleExNohlsearchCommand(const ExCommand &cmd);
    bool handleExNormalCommand(const ExCommand &cmd);
    bool handleExReadCommand(const ExCommand &cmd);
    bool handleExUndoRedoCommand(const ExCommand &cmd);
    bool handleExSetCommand(const ExCommand &cmd);
    bool handleExSortCommand(const ExCommand &cmd);
    bool handleExShiftCommand(const ExCommand &cmd);
    bool handleExSourceCommand(const ExCommand &cmd);
    bool handleExSubstituteCommand(const ExCommand &cmd);
    bool handleExTabNextCommand(const ExCommand &cmd);
    bool handleExTabPreviousCommand(const ExCommand &cmd);
    bool handleExWriteCommand(const ExCommand &cmd);
    bool handleExEchoCommand(const ExCommand &cmd);

    void setTabSize(int tabSize);
    void setupCharClass();
    int charClass(QChar c, bool simple) const;
    signed char m_charClass[256];

    int m_ctrlVAccumulator;
    int m_ctrlVLength;
    int m_ctrlVBase;

    QTimer m_fixCursorTimer;
    QTimer m_inputTimer;

    void miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos);

    // Data shared among editors with same document.
    struct BufferData
    {
        QStack<State> undo;
        QStack<State> redo;
        State undoState;
        int lastRevision = 0;

        int editBlockLevel = 0; // current level of edit blocks
        bool breakEditBlock = false; // if true, joinPreviousEditBlock() starts new edit block

        QStack<CursorPosition> jumpListUndo;
        QStack<CursorPosition> jumpListRedo;

        VisualMode lastVisualMode = NoVisualMode;
        bool lastVisualModeInverted = false;

        Marks marks;

        // Insert state to get last inserted text.
        struct InsertState {
            int pos1;
            int pos2;
            int backspaces;
            int deletes;
            QSet<int> spaces;
            bool insertingSpaces;
            QString textBeforeCursor;
            bool newLineBefore;
            bool newLineAfter;
        } insertState;

        QString lastInsertion;

        // If there are multiple editors with same document,
        // only the handler with last focused editor can change buffer data.
        QPointer<FakeVimHandler::Private> currentHandler;
    };

    using BufferDataPtr = QSharedPointer<BufferData>;
    void pullOrCreateBufferData();
    BufferDataPtr m_buffer;

    // Data shared among all editors.
    static struct GlobalData
    {
        GlobalData()
            : mappings()
            , currentMap(&mappings)
        {
            commandBuffer.setPrompt(':');
        }

        // Current state.
        bool passing = false; // let the core see the next event
        Mode mode = CommandMode;
        SubMode submode = NoSubMode;
        SubSubMode subsubmode = NoSubSubMode;
        Input subsubdata;
        VisualMode visualMode = NoVisualMode;
        Input minibufferData;

        // [count] for current command, 0 if no [count] available
        int mvcount = 0;
        int opcount = 0;

        MoveType movetype = MoveInclusive;
        RangeMode rangemode = RangeCharMode;
        bool gflag = false;  // whether current command started with 'g'

        // Extra data for ';'.
        Input semicolonType;  // 'f', 'F', 't', 'T'
        QString semicolonKey;

        // Repetition.
        QString dotCommand;

        QHash<int, Register> registers;

        // All mappings.
        Mappings mappings;

        // Input.
        QList<Input> pendingInput;
        MappingsIterator currentMap;
        QStack<MappingState> mapStates;
        int mapDepth = 0;

        // Command line buffers.
        CommandBuffer commandBuffer;
        CommandBuffer searchBuffer;

        // Current mini buffer message.
        QString currentMessage;
        MessageLevel currentMessageLevel = MessageInfo;
        QString currentCommand;

        // Search state.
        QString lastSearch; // last search expression as entered by user
        QString lastNeedle; // last search expression translated with vimPatternToQtPattern()
        bool lastSearchForward = false; // last search command was '/' or '*'
        bool highlightsCleared = false; // ':nohlsearch' command is active until next search
        bool findPending = false; // currently searching using external tool (until editor is focused again)

        // Last substitution command.
        QString lastSubstituteFlags;
        QString lastSubstitutePattern;
        QString lastSubstituteReplacement;

        // Global marks.
        Marks marks;

        // Return to insert/replace mode after single command (<C-O>).
        Mode returnToMode = CommandMode;

        // Currently recorded macro
        bool isRecording = false;
        QString recorded;
        int currentRegister = 0;
        int lastExecutedRegister = 0;
    } g;
};

FakeVimHandler::Private::GlobalData FakeVimHandler::Private::g;

FakeVimHandler::Private::Private(FakeVimHandler *parent, QWidget *widget)
{
    q = parent;
    m_textedit = qobject_cast<QTextEdit *>(widget);
    m_plaintextedit = qobject_cast<QPlainTextEdit *>(widget);

    init();

    if (editor()) {
        connect(EDITOR(document()), &QTextDocument::contentsChange,
                this, &Private::onContentsChanged);
        connect(EDITOR(document()), &QTextDocument::undoCommandAdded,
                this, &Private::onUndoCommandAdded);
        m_buffer->lastRevision = revision();
    }
}

void FakeVimHandler::Private::init()
{
    m_cursor = QTextCursor(document());
    m_cursorNeedsUpdate = true;
    m_inFakeVim = false;
    m_findStartPosition = -1;
    m_visualBlockInsert = NoneBlockInsertMode;
    m_positionPastEnd = false;
    m_anchorPastEnd = false;
    m_register = '"';
    m_targetColumn = 0;
    m_visualTargetColumn = 0;
    m_targetColumnWrapped = 0;
    m_searchStartPosition = 0;
    m_searchFromScreenLine = 0;
    m_firstVisibleLine = 0;
    m_ctrlVAccumulator = 0;
    m_ctrlVLength = 0;
    m_ctrlVBase = 0;

    initSingleShotTimer(&m_fixCursorTimer, 0, this, &FakeVimHandler::Private::onFixCursorTimeout);
    initSingleShotTimer(&m_inputTimer, 1000, this, &FakeVimHandler::Private::onInputTimeout);

    pullOrCreateBufferData();
    setupCharClass();
}

void FakeVimHandler::Private::focus()
{
    m_buffer->currentHandler = this;

    enterFakeVim();

    stopIncrementalFind();
    if (isCommandLineMode()) {
        if (g.subsubmode == SearchSubSubMode) {
            setPosition(m_searchStartPosition);
            scrollToLine(m_searchFromScreenLine);
        } else {
            leaveVisualMode();
            setPosition(qMin(position(), anchor()));
        }
        leaveCurrentMode();
        setTargetColumn();
        setAnchor();
        commitCursor();
    } else {
        clearCurrentMode();
    }
    fixExternalCursor(true);
    updateHighlights();

    leaveFakeVim(false);
}

void FakeVimHandler::Private::unfocus()
{
    fixExternalCursor(false);
}

void FakeVimHandler::Private::fixExternalCursor(bool focus)
{
    m_fixCursorTimer.stop();

    if (isVisualCharMode() && !focus && !hasThinCursor()) {
        // Select the character under thick cursor for external operations with text selection.
        fixExternalCursorPosition(false);
    } else if (isVisualCharMode() && focus && hasThinCursor()) {
        // Fix cursor position if changing its shape.
        // The fix is postponed so context menu action can be finished.
        m_fixCursorTimer.start();
    } else {
        updateCursorShape();
    }
}

void FakeVimHandler::Private::fixExternalCursorPosition(bool focus)
{
    QTextCursor tc = editorCursor();
    if (tc.anchor() < tc.position()) {
        tc.movePosition(focus ? Left : Right, KeepAnchor);
        EDITOR(setTextCursor(tc));
    }

    setThinCursor(!focus);
}

void FakeVimHandler::Private::enterFakeVim()
{
    if (m_inFakeVim) {
        qWarning("enterFakeVim() shouldn't be called recursively!");
        return;
    }

    if (!m_buffer->currentHandler)
        m_buffer->currentHandler = this;

    pullOrCreateBufferData();

    m_inFakeVim = true;

    removeEventFilter();

    pullCursor();

    updateFirstVisibleLine();
}

void FakeVimHandler::Private::leaveFakeVim(bool needUpdate)
{
    if (!m_inFakeVim) {
        qWarning("enterFakeVim() not called before leaveFakeVim()!");
        return;
    }

    // The command might have destroyed the editor.
    if (m_textedit || m_plaintextedit) {
        if (hasConfig(ConfigShowMarks))
            updateSelection();

        updateMiniBuffer();

        if (needUpdate) {
            // Move cursor line to middle of screen if it's not visible.
            const int line = cursorLine();
            if (line < firstVisibleLine() || line > firstVisibleLine() + linesOnScreen())
                scrollToLine(qMax(0, line - linesOnScreen() / 2));
            else
                scrollToLine(firstVisibleLine());
            updateScrollOffset();

            commitCursor();
        }

        installEventFilter();
    }

    m_inFakeVim = false;
}

void FakeVimHandler::Private::leaveFakeVim(EventResult eventResult)
{
    leaveFakeVim(eventResult == EventHandled || eventResult == EventCancelled);
}

bool FakeVimHandler::Private::wantsOverride(QKeyEvent *ev)
{
    const int key = ev->key();
    const Qt::KeyboardModifiers mods = ev->modifiers();
    KEY_DEBUG("SHORTCUT OVERRIDE" << key << "  PASSING: " << g.passing);

    if (key == Key_Escape) {
        if (g.subsubmode == SearchSubSubMode)
            return true;
        // Not sure this feels good. People often hit Esc several times.
        if (isNoVisualMode()
                && g.mode == CommandMode
                && g.submode == NoSubMode
                && g.currentCommand.isEmpty()
                && g.returnToMode == CommandMode)
            return false;
        return true;
    }

    // We are interested in overriding most Ctrl key combinations.
    if (isOnlyControlModifier(mods)
            && !config(ConfigPassControlKey).toBool()
            && ((key >= Key_A && key <= Key_Z && key != Key_K)
                || key == Key_BracketLeft || key == Key_BracketRight)) {
        // Ctrl-K is special as it is the Core's default notion of Locator
        if (g.passing) {
            KEY_DEBUG(" PASSING CTRL KEY");
            // We get called twice on the same key
            //g.passing = false;
            return false;
        }
        KEY_DEBUG(" NOT PASSING CTRL KEY");
        return true;
    }

    // Let other shortcuts trigger.
    return false;
}

EventResult FakeVimHandler::Private::handleEvent(QKeyEvent *ev)
{
    const int key = ev->key();
    const Qt::KeyboardModifiers mods = ev->modifiers();

    if (key == Key_Shift || key == Key_Alt || key == Key_Control
            || key == Key_AltGr || key == Key_Meta)
    {
        KEY_DEBUG("PLAIN MODIFIER");
        return EventUnhandled;
    }

    if (g.passing) {
        passShortcuts(false);
        KEY_DEBUG("PASSING PLAIN KEY..." << ev->key() << ev->text());
        //if (input.is(',')) { // use ',,' to leave, too.
        //    qDebug() << "FINISHED...";
        //    return EventHandled;
        //}
        KEY_DEBUG("   PASS TO CORE");
        return EventPassedToCore;
    }

#ifndef FAKEVIM_STANDALONE
    bool inSnippetMode = false;
    QMetaObject::invokeMethod(editor(),
        "inSnippetMode", Q_ARG(bool *, &inSnippetMode));

    if (inSnippetMode)
        return EventPassedToCore;
#endif

    // Fake "End of line"
    //m_tc = m_cursor;

    //bool hasBlock = false;
    //q->requestHasBlockSelection(&hasBlock);
    //qDebug() << "IMPORT BLOCK 2:" << hasBlock;

    //if (0 && hasBlock) {
    //    (pos > anc) ? --pos : --anc;

    //if ((mods & RealControlModifier) != 0) {
    //    if (key >= Key_A && key <= Key_Z)
    //        key = shift(key); // make it lower case
    //    key = control(key);
    //} else if (key >= Key_A && key <= Key_Z && (mods & Qt::ShiftModifier) == 0) {
    //    key = shift(key);
    //}

    //QTC_ASSERT(g.mode == InsertMode || g.mode == ReplaceMode
    //        || !atBlockEnd() || block().length() <= 1,
    //    qDebug() << "Cursor at EOL before key handler");

    const Input input(key, mods, ev->text());
    if (!input.isValid())
        return EventUnhandled;

    enterFakeVim();
    EventResult result = handleKey(input);
    leaveFakeVim(result);
    return result;
}

void FakeVimHandler::Private::installEventFilter()
{
    EDITOR(installEventFilter(q));
}

void FakeVimHandler::Private::removeEventFilter()
{
    EDITOR(removeEventFilter(q));
}

void FakeVimHandler::Private::setupWidget()
{
    m_cursorNeedsUpdate = true;
    if (m_textedit) {
        connect(m_textedit, &QTextEdit::cursorPositionChanged,
                this, &FakeVimHandler::Private::onCursorPositionChanged, Qt::UniqueConnection);
    } else {
        connect(m_plaintextedit, &QPlainTextEdit::cursorPositionChanged,
                this, &FakeVimHandler::Private::onCursorPositionChanged, Qt::UniqueConnection);
    }

    enterFakeVim();

    leaveCurrentMode();
    m_wasReadOnly = EDITOR(isReadOnly());

    updateEditor();

    leaveFakeVim();
}

void FakeVimHandler::Private::commitInsertState()
{
    if (!isInsertStateValid())
        return;

    QString &lastInsertion = m_buffer->lastInsertion;
    BufferData::InsertState &insertState = m_buffer->insertState;

    // Get raw inserted text.
    lastInsertion = textAt(insertState.pos1, insertState.pos2);

    // Escape special characters and spaces inserted by user (not by auto-indentation).
    for (int i = lastInsertion.size() - 1; i >= 0; --i) {
        const int pos = insertState.pos1 + i;
        const QChar c = characterAt(pos);
        if (c == '<')
            lastInsertion.replace(i, 1, "<LT>");
        else if ((c == ' ' || c == '\t') && insertState.spaces.contains(pos))
            lastInsertion.replace(i, 1, QLatin1String(c == ' ' ? "<SPACE>" : "<TAB>"));
    }

    // Remove unnecessary backspaces.
    while (insertState.backspaces > 0 && !lastInsertion.isEmpty() && lastInsertion[0].isSpace())
        --insertState.backspaces;

    // backspaces in front of inserted text
    lastInsertion.prepend(QString("<BS>").repeated(insertState.backspaces));
    // deletes after inserted text
    lastInsertion.prepend(QString("<DELETE>").repeated(insertState.deletes));

    // Remove indentation.
    lastInsertion.replace(QRegularExpression("(^|\n)[\\t ]+"), "\\1");
}

void FakeVimHandler::Private::invalidateInsertState()
{
    BufferData::InsertState &insertState = m_buffer->insertState;
    insertState.pos1 = -1;
    insertState.pos2 = position();
    insertState.backspaces = 0;
    insertState.deletes = 0;
    insertState.spaces.clear();
    insertState.insertingSpaces = false;
    insertState.textBeforeCursor = textAt(block().position(), position());
    insertState.newLineBefore = false;
    insertState.newLineAfter = false;
}

bool FakeVimHandler::Private::isInsertStateValid() const
{
    return m_buffer->insertState.pos1 != -1;
}

void FakeVimHandler::Private::clearLastInsertion()
{
    invalidateInsertState();
    m_buffer->lastInsertion.clear();
    m_buffer->insertState.pos1 = m_buffer->insertState.pos2;
}

void FakeVimHandler::Private::ensureCursorVisible()
{
    int pos = position();
    int anc = isVisualMode() ? anchor() : position();

    // fix selection so it is outside folded block
    int start = qMin(pos, anc);
    int end = qMax(pos, anc) + 1;
    QTextBlock block = blockAt(start);
    QTextBlock block2 = blockAt(end);
    if (!block.isVisible() || !block2.isVisible()) {
        // FIXME: Moving cursor left/right or unfolding block immediately after block is folded
        //        should restore cursor position inside block.
        // Changing cursor position after folding is not Vim behavior so at least record the jump.
        if (block.isValid() && !block.isVisible())
            recordJump();

        pos = start;
        while (block.isValid() && !block.isVisible())
            block = block.previous();
        if (block.isValid())
            pos = block.position() + qMin(m_targetColumn, block.length() - 2);

        if (isVisualMode()) {
            anc = end;
            while (block2.isValid() && !block2.isVisible()) {
                anc = block2.position() + block2.length() - 2;
                block2 = block2.next();
            }
        }

        setAnchorAndPosition(anc, pos);
    }
}

void FakeVimHandler::Private::updateEditor()
{
    setTabSize(config(ConfigTabStop).toInt());
    setupCharClass();
}

void FakeVimHandler::Private::setTabSize(int tabSize)
{
    const QLatin1Char space(' ');
    const QFontMetricsF metrics(EDITOR(fontMetrics()));

#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
    const qreal width = metrics.horizontalAdvance(QString(tabSize, space));
#else
    const qreal width = metrics.width(space) * tabSize;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
    EDITOR(setTabStopDistance(width));
#else
    EDITOR(setTabStopWidth(width));
#endif
}

void FakeVimHandler::Private::restoreWidget(int tabSize)
{
    //EDITOR(removeEventFilter(q));
    //EDITOR(setReadOnly(m_wasReadOnly));
    setTabSize(tabSize);
    g.visualMode = NoVisualMode;
    // Force "ordinary" cursor.
    setThinCursor();
    updateSelection();
    updateHighlights();
    if (m_textedit) {
        disconnect(m_textedit, &QTextEdit::cursorPositionChanged,
                   this, &FakeVimHandler::Private::onCursorPositionChanged);
    } else {
        disconnect(m_plaintextedit, &QPlainTextEdit::cursorPositionChanged,
                   this, &FakeVimHandler::Private::onCursorPositionChanged);
    }
}

EventResult FakeVimHandler::Private::handleKey(const Input &input)
{
    KEY_DEBUG("HANDLE INPUT: " << input);

    bool hasInput = input.isValid();

    // Waiting on input to complete mapping?
    EventResult r = stopWaitForMapping(hasInput);

    if (hasInput) {
        record(input);
        g.pendingInput.append(input);
    }

    // Process pending input.
    // Note: Pending input is global state and can be extended by:
    //         1. handling a user input (though handleKey() is not called recursively),
    //         2. expanding a user mapping or
    //         3. executing a register.
    while (!g.pendingInput.isEmpty() && r == EventHandled) {
        const Input in = g.pendingInput.takeFirst();

        // invalid input is used to pop mapping state
        if (!in.isValid()) {
            endMapping();
        } else {
            // Handle user mapping.
            if (canHandleMapping()) {
                if (extendMapping(in)) {
                    if (!hasInput || !g.currentMap.canExtend())
                        expandCompleteMapping();
                } else if (!expandCompleteMapping()) {
                    r = handleCurrentMapAsDefault();
                }
            } else {
                r = handleDefaultKey(in);
            }
        }
    }

    if (g.currentMap.canExtend()) {
        waitForMapping();
        return EventHandled;
    }

    if (r != EventHandled)
        clearPendingInput();

    return r;
}

bool FakeVimHandler::Private::handleCommandBufferPaste(const Input &input)
{
    if (input.isControl('r')
        && (g.subsubmode == SearchSubSubMode || g.mode == ExMode)) {
        g.minibufferData = input;
        return true;
    }
    if (g.minibufferData.isControl('r')) {
        g.minibufferData = Input();
        if (input.isEscape())
            return true;
        CommandBuffer &buffer = (g.subsubmode == SearchSubSubMode)
            ? g.searchBuffer : g.commandBuffer;
        if (input.isControl('w')) {
            QTextCursor tc = m_cursor;
            tc.select(QTextCursor::WordUnderCursor);
            QString word = tc.selectedText();
            buffer.insertText(word);
        } else {
            QString r = registerContents(input.asChar().unicode());
            buffer.insertText(r);
        }
        updateMiniBuffer();
        return true;
    }
    return false;
}

EventResult FakeVimHandler::Private::handleDefaultKey(const Input &input)
{
    if (g.passing) {
        passShortcuts(false);
        QKeyEvent event(QEvent::KeyPress, input.key(), input.modifiers(), input.text());
        bool accepted = QApplication::sendEvent(editor()->window(), &event);
        if (accepted || (!m_textedit && !m_plaintextedit))
            return EventHandled;
    }

    if (input == Nop)
        return EventHandled;
    else if (g.subsubmode == SearchSubSubMode)
        return handleSearchSubSubMode(input);
    else if (g.mode == CommandMode)
        return handleCommandMode(input);
    else if (g.mode == InsertMode || g.mode == ReplaceMode)
        return handleInsertOrReplaceMode(input);
    else if (g.mode == ExMode)
        return handleExMode(input);
    return EventUnhandled;
}

EventResult FakeVimHandler::Private::handleCurrentMapAsDefault()
{
    // If mapping has failed take the first input from it and try default command.
    const Inputs &inputs = g.currentMap.currentInputs();
    if (inputs.isEmpty())
        return EventHandled;

    Input in = inputs.front();
    if (inputs.size() > 1)
        prependInputs(inputs.mid(1));
    g.currentMap.reset();

    return handleDefaultKey(in);
}

void FakeVimHandler::Private::prependInputs(const QVector<Input> &inputs)
{
    for (int i = inputs.size() - 1; i >= 0; --i)
        g.pendingInput.prepend(inputs[i]);
}

void FakeVimHandler::Private::prependMapping(const Inputs &inputs)
{
    // FIXME: Implement Vim option maxmapdepth (default value is 1000).
    if (g.mapDepth >= 1000) {
        const int i = qMax(0, g.pendingInput.lastIndexOf(Input()));
        QList<Input> inputs = g.pendingInput.mid(i);
        clearPendingInput();
        g.pendingInput.append(inputs);
        showMessage(MessageError, Tr::tr("Recursive mapping"));
        return;
    }

    ++g.mapDepth;
    g.pendingInput.prepend(Input());
    prependInputs(inputs);
    g.commandBuffer.setHistoryAutoSave(false);

    // start new edit block (undo/redo) only if necessary
    bool editBlock = m_buffer->editBlockLevel == 0 && !(isInsertMode() && isInsertStateValid());
    if (editBlock)
        beginLargeEditBlock();
    g.mapStates << MappingState(inputs.noremap(), inputs.silent(), editBlock);
}

bool FakeVimHandler::Private::expandCompleteMapping()
{
    if (!g.currentMap.isComplete())
        return false;

    const Inputs &inputs = g.currentMap.inputs();
    int usedInputs = g.currentMap.mapLength();
    prependInputs(g.currentMap.currentInputs().mid(usedInputs));
    prependMapping(inputs);
    g.currentMap.reset();

    return true;
}

bool FakeVimHandler::Private::extendMapping(const Input &input)
{
    if (!g.currentMap.isValid())
        g.currentMap.reset(currentModeCode());
    return g.currentMap.walk(input);
}

void FakeVimHandler::Private::endMapping()
{
    if (!g.currentMap.canExtend())
        --g.mapDepth;
    if (g.mapStates.isEmpty())
        return;
    if (g.mapStates.last().editBlock)
        endEditBlock();
    g.mapStates.pop_back();
    if (g.mapStates.isEmpty())
        g.commandBuffer.setHistoryAutoSave(true);
}

bool FakeVimHandler::Private::canHandleMapping()
{
    // Don't handle user mapping in sub-modes that cannot be followed by movement and in "noremap".
    return g.subsubmode == NoSubSubMode
        && g.submode != RegisterSubMode
        && g.submode != WindowSubMode
        && g.submode != ZSubMode
        && g.submode != CapitalZSubMode
        && g.submode != ReplaceSubMode
        && g.submode != MacroRecordSubMode
        && g.submode != MacroExecuteSubMode
        && (g.mapStates.isEmpty() || !g.mapStates.last().noremap);
}

void FakeVimHandler::Private::clearPendingInput()
{
    // Clear pending input on interrupt or bad mapping.
    g.pendingInput.clear();
    g.mapStates.clear();
    g.mapDepth = 0;

    // Clear all started edit blocks.
    while (m_buffer->editBlockLevel > 0)
        endEditBlock();
}

void FakeVimHandler::Private::waitForMapping()
{
    g.currentCommand.clear();
    foreach (const Input &input, g.currentMap.currentInputs())
        g.currentCommand.append(input.toString());

    // wait for user to press any key or trigger complete mapping after interval
    m_inputTimer.start();
}

EventResult FakeVimHandler::Private::stopWaitForMapping(bool hasInput)
{
    if (!hasInput || m_inputTimer.isActive()) {
        m_inputTimer.stop();
        g.currentCommand.clear();
        if (!hasInput && !expandCompleteMapping()) {
            // Cannot complete mapping so handle the first input from it as default command.
            return handleCurrentMapAsDefault();
        }
    }

    return EventHandled;
}

void FakeVimHandler::Private::stopIncrementalFind()
{
    if (g.findPending) {
        g.findPending = false;
        setAnchorAndPosition(m_findStartPosition, m_cursor.selectionStart());
        finishMovement();
        setAnchor();
    }
}

void FakeVimHandler::Private::updateFind(bool isComplete)
{
    if (!isComplete && !hasConfig(ConfigIncSearch))
        return;

    g.currentMessage.clear();

    const QString &needle = g.searchBuffer.contents();
    if (isComplete) {
        setPosition(m_searchStartPosition);
        if (!needle.isEmpty())
            recordJump();
    }

    SearchData sd;
    sd.needle = needle;
    sd.forward = g.lastSearchForward;
    sd.highlightMatches = isComplete;
    search(sd, isComplete);
}

void FakeVimHandler::Private::resetCount()
{
    g.mvcount = 0;
    g.opcount = 0;
}

bool FakeVimHandler::Private::isInputCount(const Input &input) const
{
    return input.isDigit() && (!input.is('0') || g.mvcount > 0);
}

bool FakeVimHandler::Private::atEmptyLine(int pos) const
{
    return blockAt(pos).length() == 1;
}

bool FakeVimHandler::Private::atEmptyLine(const QTextCursor &tc) const
{
    return atEmptyLine(tc.position());
}

bool FakeVimHandler::Private::atEmptyLine() const
{
    return atEmptyLine(position());
}

bool FakeVimHandler::Private::atBoundary(bool end, bool simple, bool onlyWords,
    const QTextCursor &tc) const
{
    if (tc.isNull())
        return atBoundary(end, simple, onlyWords, m_cursor);
    if (atEmptyLine(tc))
        return true;
    int pos = tc.position();
    QChar c1 = characterAt(pos);
    QChar c2 = characterAt(pos + (end ? 1 : -1));
    int thisClass = charClass(c1, simple);
    return (!onlyWords || thisClass != 0)
        && (c2.isNull() || c2 == ParagraphSeparator || thisClass != charClass(c2, simple));
}

bool FakeVimHandler::Private::atWordBoundary(bool end, bool simple, const QTextCursor &tc) const
{
    return atBoundary(end, simple, true, tc);
}

bool FakeVimHandler::Private::atWordStart(bool simple, const QTextCursor &tc) const
{
    return atWordBoundary(false, simple, tc);
}

bool FakeVimHandler::Private::atWordEnd(bool simple, const QTextCursor &tc) const
{
    return atWordBoundary(true, simple, tc);
}

bool FakeVimHandler::Private::isFirstNonBlankOnLine(int pos)
{
    for (int i = blockAt(pos).position(); i < pos; ++i) {
        if (!document()->characterAt(i).isSpace())
            return false;
    }
    return true;
}

void FakeVimHandler::Private::pushUndoState(bool overwrite)
{
    if (m_buffer->editBlockLevel != 0 && m_buffer->undoState.isValid())
        return; // No need to save undo state for inner edit blocks.

    if (m_buffer->undoState.isValid() && !overwrite)
        return;

    UNDO_DEBUG("PUSH UNDO");
    int pos = position();
    if (!isInsertMode()) {
        if (isVisualMode() || g.submode == DeleteSubMode
            || (g.submode == ChangeSubMode && g.movetype != MoveLineWise)) {
            pos = qMin(pos, anchor());
            if (isVisualLineMode())
                pos = firstPositionInLine(lineForPosition(pos));
            else if (isVisualBlockMode())
                pos = blockAt(pos).position() + qMin(columnAt(anchor()), columnAt(position()));
        } else if (g.movetype == MoveLineWise && hasConfig(ConfigStartOfLine)) {
            QTextCursor tc = m_cursor;
            if (g.submode == ShiftLeftSubMode || g.submode == ShiftRightSubMode
                || g.submode == IndentSubMode) {
                pos = qMin(pos, anchor());
            }
            tc.setPosition(pos);
            moveToFirstNonBlankOnLine(&tc);
            pos = qMin(pos, tc.position());
        }
    }

    CursorPosition lastChangePosition(document(), pos);
    setMark('.', lastChangePosition);

    m_buffer->redo.clear();
    m_buffer->undoState = State(
                revision(), lastChangePosition, m_buffer->marks,
                m_buffer->lastVisualMode, m_buffer->lastVisualModeInverted);
}

void FakeVimHandler::Private::moveDown(int n)
{
    if (n == 0)
        return;

    QTextBlock block = m_cursor.block();
    const int col = position() - block.position();

    int lines = qAbs(n);
    int position = 0;
    while (block.isValid()) {
        position = block.position() + qMax(0, qMin(block.length() - 2, col));
        if (block.isVisible()) {
            --lines;
            if (lines < 0)
                break;
        }
        block = n > 0 ? nextLine(block) : previousLine(block);
    }

    setPosition(position);
    moveToTargetColumn();
    updateScrollOffset();
}

void FakeVimHandler::Private::moveDownVisually(int n)
{
    const QTextCursor::MoveOperation moveOperation = (n > 0) ? Down : Up;
    int count = qAbs(n);
    int oldPos = m_cursor.position();

    while (count > 0) {
        m_cursor.movePosition(moveOperation, KeepAnchor, 1);
        if (oldPos == m_cursor.position())
            break;
        oldPos = m_cursor.position();
        QTextBlock block = m_cursor.block();
        if (block.isVisible())
            --count;
    }

    QTextCursor tc = m_cursor;
    tc.movePosition(StartOfLine);
    const int minPos = tc.position();
    moveToEndOfLineVisually(&tc);
    const int maxPos = tc.position();

    if (m_targetColumn == -1) {
        setPosition(maxPos);
    } else {
        setPosition(qMin(maxPos, minPos + m_targetColumnWrapped));
        const int targetColumn = m_targetColumnWrapped;
        setTargetColumn();
        m_targetColumnWrapped = targetColumn;
    }

    if (!isInsertMode() && atEndOfLine())
        m_cursor.movePosition(Left, KeepAnchor);

    updateScrollOffset();
}

void FakeVimHandler::Private::movePageDown(int count)
{
    const int scrollOffset = windowScrollOffset();
    const int screenLines = linesOnScreen();
    const int offset = count > 0 ? scrollOffset - 2 : screenLines - scrollOffset + 2;
    const int value = count * screenLines - cursorLineOnScreen() + offset;
    moveDown(value);

    if (count > 0)
        scrollToLine(cursorLine());
    else
        scrollToLine(qMax(0, cursorLine() - screenLines + 1));
}

void FakeVimHandler::Private::commitCursor()
{
    QTextCursor tc = m_cursor;

    if (isVisualMode()) {
        int pos = tc.position();
        int anc = tc.anchor();

        if (isVisualBlockMode()) {
            const int col1 = columnAt(anc);
            const int col2 = columnAt(pos);
            if (col1 > col2)
                ++anc;
            else if (!tc.atBlockEnd())
                ++pos;
            // FIXME: After '$' command (i.e. m_visualTargetColumn == -1), end of selected lines
            //        should be selected.
        } else if (isVisualLineMode()) {
            const int posLine = lineForPosition(pos);
            const int ancLine = lineForPosition(anc);
            if (anc < pos) {
                pos = lastPositionInLine(posLine);
                anc = firstPositionInLine(ancLine);
            } else {
                pos = firstPositionInLine(posLine);
                anc = lastPositionInLine(ancLine) + 1;
            }
            // putting cursor on folded line will unfold the line, so move the cursor a bit
            if (!blockAt(pos).isVisible())
                ++pos;
        } else if (isVisualCharMode()) {
            if (anc > pos)
                ++anc;
            else if (!editor()->hasFocus() || isCommandLineMode())
                m_fixCursorTimer.start();
        }

        tc.setPosition(anc);
        tc.setPosition(pos, KeepAnchor);
    } else if (g.subsubmode == SearchSubSubMode && !m_searchCursor.isNull()) {
        tc = m_searchCursor;
    } else {
        tc.clearSelection();
    }

    updateCursorShape();

    if (isVisualBlockMode()) {
        q->requestSetBlockSelection(tc);
    } else  {
        q->requestDisableBlockSelection();
        if (editor())
            EDITOR(setTextCursor(tc));
    }
}

void FakeVimHandler::Private::pullCursor()
{
    if (!m_cursorNeedsUpdate)
        return;

    m_cursorNeedsUpdate = false;

    QTextCursor oldCursor = m_cursor;

    bool visualBlockMode = false;
    q->requestHasBlockSelection(&visualBlockMode);

    if (visualBlockMode)
        q->requestBlockSelection(&m_cursor);
    else if (editor())
        m_cursor = editorCursor();

    // Cursor should be always valid.
    if (m_cursor.isNull())
        m_cursor = QTextCursor(document());

    if (visualBlockMode)
        g.visualMode = VisualBlockMode;
    else if (m_cursor.hasSelection())
        g.visualMode = VisualCharMode;
    else
        g.visualMode = NoVisualMode;

    // Keep visually the text selection same.
    // With thick text cursor, the character under cursor is treated as selected.
    if (isVisualCharMode() && hasThinCursor())
        moveLeft();

    // Cursor position can be after the end of line only in some modes.
    if (atEndOfLine() && !isVisualMode() && !isInsertMode())
        moveLeft();

    // Record external jump to different line.
    if (lineForPosition(position()) != lineForPosition(oldCursor.position()))
        recordJump(oldCursor.position());

    setTargetColumn();
}

QTextCursor FakeVimHandler::Private::editorCursor() const
{
    QTextCursor tc = EDITOR(textCursor());
    tc.setVisualNavigation(false);
    return tc;
}

bool FakeVimHandler::Private::moveToNextParagraph(int count)
{
    const bool forward = count > 0;
    int repeat = forward ? count : -count;
    QTextBlock block = this->block();

    if (block.isValid() && block.length() == 1)
        ++repeat;

    for (; block.isValid(); block = forward ? block.next() : block.previous()) {
        if (block.length() == 1) {
            if (--repeat == 0)
                break;
            while (block.isValid() && block.length() == 1)
                block = forward ? block.next() : block.previous();
            if (!block.isValid())
                break;
        }
    }

    if (!block.isValid())
        --repeat;

    if (repeat > 0)
        return false;

    if (block.isValid())
        setPosition(block.position());
    else
        setPosition(forward ? lastPositionInDocument() : 0);

    return true;
}

void FakeVimHandler::Private::moveToParagraphStartOrEnd(int direction)
{
    bool emptyLine = atEmptyLine();
    int oldPos = -1;

    while (atEmptyLine() == emptyLine && oldPos != position()) {
        oldPos = position();
        moveDown(direction);
    }

    if (oldPos != position())
        moveUp(direction);
}

void FakeVimHandler::Private::moveToEndOfLine()
{
    // Additionally select (in visual mode) or apply current command on hidden lines following
    // the current line.
    bool onlyVisibleLines = isVisualMode() || g.submode != NoSubMode;
    const int id = onlyVisibleLines ? lineNumber(block()) : block().blockNumber() + 1;
    setPosition(lastPositionInLine(id, onlyVisibleLines));
    setTargetColumn();
}

void FakeVimHandler::Private::moveToEndOfLineVisually()
{
    moveToEndOfLineVisually(&m_cursor);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToEndOfLineVisually(QTextCursor *tc)
{
    // Moving to end of line ends up on following line if the line is wrapped.
    tc->movePosition(StartOfLine);
    const int minPos = tc->position();
    tc->movePosition(EndOfLine);
    int maxPos = tc->position();
    tc->movePosition(StartOfLine);
    if (minPos != tc->position())
        --maxPos;
    tc->setPosition(maxPos);
}

void FakeVimHandler::Private::moveBehindEndOfLine()
{
    q->fold(1, false);
    int pos = qMin(block().position() + block().length() - 1,
        lastPositionInDocument() + 1);
    setPosition(pos);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToStartOfLine()
{
    setPosition(block().position());
    setTargetColumn();
}

void FakeVimHandler::Private::moveToStartOfLineVisually()
{
    m_cursor.movePosition(StartOfLine, KeepAnchor);
    setTargetColumn();
}

void FakeVimHandler::Private::fixSelection()
{
    if (g.rangemode == RangeBlockMode)
         return;

    if (g.movetype == MoveInclusive) {
        // If position or anchor is after end of non-empty line, include line break in selection.
        if (characterAtCursor() == ParagraphSeparator) {
            if (!atEmptyLine() && !atDocumentEnd()) {
                setPosition(position() + 1);
                return;
            }
        } else if (characterAt(anchor()) == ParagraphSeparator) {
            QTextCursor tc = m_cursor;
            tc.setPosition(anchor());
            if (!atEmptyLine(tc)) {
                setAnchorAndPosition(anchor() + 1, position());
                return;
            }
        }
    }

    if (g.movetype == MoveExclusive && g.subsubmode == NoSubSubMode) {
        if (anchor() < position() && atBlockStart()) {
            // Exclusive motion ending at the beginning of line
            // becomes inclusive and end is moved to end of previous line.
            g.movetype = MoveInclusive;
            moveToStartOfLine();
            moveLeft();

            // Exclusive motion ending at the beginning of line and
            // starting at or before first non-blank on a line becomes linewise.
            if (anchor() < block().position() && isFirstNonBlankOnLine(anchor()))
                g.movetype = MoveLineWise;
        }
    }

    if (g.movetype == MoveLineWise)
        g.rangemode = (g.submode == ChangeSubMode)
            ? RangeLineModeExclusive
            : RangeLineMode;

    if (g.movetype == MoveInclusive) {
        if (anchor() <= position()) {
            if (!atBlockEnd())
                setPosition(position() + 1); // correction

            // Omit first character in selection if it's line break on non-empty line.
            int start = anchor();
            int end = position();
            if (afterEndOfLine(document(), start) && start > 0) {
                start = qMin(start + 1, end);
                if (g.submode == DeleteSubMode && !atDocumentEnd())
                    setAnchorAndPosition(start, end + 1);
                else
                    setAnchorAndPosition(start, end);
            }

            // If more than one line is selected and all are selected completely
            // movement becomes linewise.
            if (start < block().position() && isFirstNonBlankOnLine(start) && atBlockEnd()) {
                if (g.submode != ChangeSubMode) {
                    moveRight();
                    if (atEmptyLine())
                        moveRight();
                }
                g.movetype = MoveLineWise;
            }
        } else if (!m_anchorPastEnd) {
            setAnchorAndPosition(anchor() + 1, position());
        }
    }

    if (m_positionPastEnd) {
        moveBehindEndOfLine();
        moveRight();
        setAnchorAndPosition(anchor(), position());
    }

    if (m_anchorPastEnd) {
        const int pos = position();
        setPosition(anchor());
        moveBehindEndOfLine();
        moveRight();
        setAnchorAndPosition(position(), pos);
    }
}

bool FakeVimHandler::Private::finishSearch()
{
    if (g.lastSearch.isEmpty()
        || (!g.currentMessage.isEmpty() && g.currentMessageLevel == MessageError)) {
        return false;
    }
    if (g.submode != NoSubMode)
        setAnchorAndPosition(m_searchStartPosition, position());
    return true;
}

void FakeVimHandler::Private::finishMovement(const QString &dotCommandMovement)
{
    //dump("FINISH MOVEMENT");
    if (g.submode == FilterSubMode) {
        int beginLine = lineForPosition(anchor());
        int endLine = lineForPosition(position());
        setPosition(qMin(anchor(), position()));
        enterExMode(QString(".,+%1!").arg(qAbs(endLine - beginLine)));
        return;
    }

    if (g.submode == ChangeSubMode
        || g.submode == DeleteSubMode
        || g.submode == YankSubMode
        || g.submode == InvertCaseSubMode
        || g.submode == DownCaseSubMode
        || g.submode == UpCaseSubMode
        || g.submode == IndentSubMode
        || g.submode == ShiftLeftSubMode
        || g.submode == ShiftRightSubMode)
    {
        fixSelection();

        if (g.submode == ChangeSubMode
            || g.submode == DeleteSubMode
            || g.submode == YankSubMode)
        {
            yankText(currentRange(), m_register);
        }
    }

    if (g.submode == ChangeSubMode) {
        pushUndoState(false);
        beginEditBlock();
        removeText(currentRange());
        if (g.movetype == MoveLineWise)
            insertAutomaticIndentation(true);
        endEditBlock();
        setTargetColumn();
    } else if (g.submode == DeleteSubMode) {
        pushUndoState(false);
        beginEditBlock();
        const int pos = position();
        // Always delete something (e.g. 'dw' on an empty line deletes the line).
        if (pos == anchor() && g.movetype == MoveInclusive)
            removeText(Range(pos, pos + 1));
        else
            removeText(currentRange());
        if (g.movetype == MoveLineWise)
            handleStartOfLine();
        endEditBlock();
    } else if (g.submode == YankSubMode) {
        bool isVisualModeYank = isVisualMode();
        leaveVisualMode();
        const QTextCursor tc = m_cursor;
        if (g.rangemode == RangeBlockMode) {
            const int pos1 = tc.block().position();
            const int pos2 = blockAt(tc.anchor()).position();
            const int col = qMin(tc.position() - pos1, tc.anchor() - pos2);
            setPosition(qMin(pos1, pos2) + col);
        } else {
            setPosition(qMin(position(), anchor()));
            if (g.rangemode == RangeLineMode) {
                if (isVisualModeYank)
                    moveToStartOfLine();
                else
                    moveToTargetColumn();
            }
        }
        setTargetColumn();
    } else if (g.submode == InvertCaseSubMode
        || g.submode == UpCaseSubMode
        || g.submode == DownCaseSubMode) {
        beginEditBlock();
        if (g.submode == InvertCaseSubMode)
            invertCase(currentRange());
        else if (g.submode == DownCaseSubMode)
            downCase(currentRange());
        else if (g.submode == UpCaseSubMode)
            upCase(currentRange());
        if (g.movetype == MoveLineWise)
            handleStartOfLine();
        endEditBlock();
    } else if (g.submode == IndentSubMode
        || g.submode == ShiftRightSubMode
        || g.submode == ShiftLeftSubMode) {
        recordJump();
        pushUndoState(false);
        if (g.submode == IndentSubMode)
            indentSelectedText();
        else if (g.submode == ShiftRightSubMode)
            shiftRegionRight(1);
        else if (g.submode == ShiftLeftSubMode)
            shiftRegionLeft(1);
    }

    if (!dotCommandMovement.isEmpty()) {
        const QString dotCommand = dotCommandFromSubMode(g.submode);
        if (!dotCommand.isEmpty())
            setDotCommand(dotCommand + dotCommandMovement);
    }

    // Change command continues in insert mode.
    if (g.submode == ChangeSubMode) {
        clearCurrentMode();
        enterInsertMode();
    } else {
        leaveCurrentMode();
    }
}

void FakeVimHandler::Private::leaveCurrentMode()
{
    if (isVisualMode())
        enterCommandMode(g.returnToMode);
    else if (g.returnToMode == CommandMode)
        enterCommandMode();
    else if (g.returnToMode == InsertMode)
        enterInsertMode();
    else
        enterReplaceMode();

    if (isNoVisualMode())
        setAnchor();
}

void FakeVimHandler::Private::clearCurrentMode()
{
    g.submode = NoSubMode;
    g.subsubmode = NoSubSubMode;
    g.movetype = MoveInclusive;
    g.gflag = false;
    m_register = '"';
    g.rangemode = RangeCharMode;
    g.currentCommand.clear();
    resetCount();
}

void FakeVimHandler::Private::updateSelection()
{
    QList<QTextEdit::ExtraSelection> selections = m_extraSelections;
    if (hasConfig(ConfigShowMarks)) {
        for (auto it = m_buffer->marks.cbegin(), end = m_buffer->marks.cend(); it != end; ++it) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = m_cursor;
            setCursorPosition(&sel.cursor, it.value().position(document()));
            sel.cursor.setPosition(sel.cursor.position(), MoveAnchor);
            sel.cursor.movePosition(Right, KeepAnchor);
            sel.format = m_cursor.blockCharFormat();
            sel.format.setForeground(Qt::blue);
            sel.format.setBackground(Qt::green);
            selections.append(sel);
        }
    }
    //qDebug() << "SELECTION: " << selections;
    q->selectionChanged(selections);
}

void FakeVimHandler::Private::updateHighlights()
{
    if (hasConfig(ConfigUseCoreSearch) || !hasConfig(ConfigHlSearch) || g.highlightsCleared) {
        if (m_highlighted.isEmpty())
            return;
        m_highlighted.clear();
    } else if (m_highlighted != g.lastNeedle) {
        m_highlighted = g.lastNeedle;
    } else {
        return;
    }

    q->highlightMatches(m_highlighted);
}

void FakeVimHandler::Private::updateMiniBuffer()
{
    if (!m_textedit && !m_plaintextedit)
        return;

    QString msg;
    int cursorPos = -1;
    int anchorPos = -1;
    MessageLevel messageLevel = MessageMode;

    if (!g.mapStates.isEmpty() && g.mapStates.last().silent && g.currentMessageLevel < MessageInfo)
        g.currentMessage.clear();

    if (g.passing) {
        msg = "PASSING";
    } else if (g.subsubmode == SearchSubSubMode) {
        msg = g.searchBuffer.display();
        if (g.mapStates.isEmpty()) {
            cursorPos = g.searchBuffer.cursorPos() + 1;
            anchorPos = g.searchBuffer.anchorPos() + 1;
        }
    } else if (g.mode == ExMode) {
        msg = g.commandBuffer.display();
        if (g.mapStates.isEmpty()) {
            cursorPos = g.commandBuffer.cursorPos() + 1;
            anchorPos = g.commandBuffer.anchorPos() + 1;
        }
    } else if (!g.currentMessage.isEmpty()) {
        msg = g.currentMessage;
        g.currentMessage.clear();
        messageLevel = g.currentMessageLevel;
    } else if (!g.mapStates.isEmpty() && !g.mapStates.last().silent) {
        // Do not reset previous message when after running a mapped command.
        return;
    } else if (g.mode == CommandMode && !g.currentCommand.isEmpty() && hasConfig(ConfigShowCmd)) {
        msg = g.currentCommand;
        messageLevel = MessageShowCmd;
    } else if (g.mode == CommandMode && isVisualMode()) {
        if (isVisualCharMode())
            msg = "-- VISUAL --";
        else if (isVisualLineMode())
            msg = "-- VISUAL LINE --";
        else if (isVisualBlockMode())
            msg = "VISUAL BLOCK";
    } else if (g.mode == InsertMode) {
        msg = "-- INSERT --";
        if (g.submode == CtrlRSubMode)
            msg += " ^R";
        else if (g.submode == CtrlVSubMode)
            msg += " ^V";
    } else if (g.mode == ReplaceMode) {
        msg = "-- REPLACE --";
    } else {
        if (g.returnToMode == CommandMode)
            msg = "-- COMMAND --";
        else if (g.returnToMode == InsertMode)
            msg = "-- (insert) --";
        else
            msg = "-- (replace) --";
    }

    if (g.isRecording && msg.startsWith("--"))
        msg.append(' ').append("Recording");

    q->commandBufferChanged(msg, cursorPos, anchorPos, messageLevel);

    int linesInDoc = linesInDocument();
    int l = cursorLine();
    QString status;
    const QString pos = QString("%1,%2")
        .arg(l + 1).arg(physicalCursorColumn() + 1);
    // FIXME: physical "-" logical
    if (linesInDoc != 0)
        status = Tr::tr("%1%2%").arg(pos, -10).arg(l * 100 / linesInDoc, 4);
    else
        status = Tr::tr("%1All").arg(pos, -10);
    q->statusDataChanged(status);
}

void FakeVimHandler::Private::showMessage(MessageLevel level, const QString &msg)
{
    //qDebug() << "MSG: " << msg;
    g.currentMessage = msg;
    g.currentMessageLevel = level;
}

void FakeVimHandler::Private::notImplementedYet()
{
    qDebug() << "Not implemented in FakeVim";
    showMessage(MessageError, Tr::tr("Not implemented in FakeVim."));
}

void FakeVimHandler::Private::passShortcuts(bool enable)
{
    g.passing = enable;
    updateMiniBuffer();
    if (enable)
        QCoreApplication::instance()->installEventFilter(q);
    else
        QCoreApplication::instance()->removeEventFilter(q);
}

bool FakeVimHandler::Private::handleCommandSubSubMode(const Input &input)
{
    bool handled = true;

    if (g.subsubmode == FtSubSubMode) {
        g.semicolonType = g.subsubdata;
        g.semicolonKey = input.text();
        handled = handleFfTt(g.semicolonKey);
        g.subsubmode = NoSubSubMode;
        if (handled) {
            finishMovement(QString("%1%2%3")
                           .arg(count())
                           .arg(g.semicolonType.text())
                           .arg(g.semicolonKey));
        }
    } else if (g.subsubmode == TextObjectSubSubMode) {
        if (input.is('w'))
            selectWordTextObject(g.subsubdata.is('i'));
        else if (input.is('W'))
            selectWORDTextObject(g.subsubdata.is('i'));
        else if (input.is('s'))
            selectSentenceTextObject(g.subsubdata.is('i'));
        else if (input.is('p'))
            selectParagraphTextObject(g.subsubdata.is('i'));
        else if (input.is('[') || input.is(']'))
            handled = selectBlockTextObject(g.subsubdata.is('i'), '[', ']');
        else if (input.is('(') || input.is(')') || input.is('b'))
            handled = selectBlockTextObject(g.subsubdata.is('i'), '(', ')');
        else if (input.is('<') || input.is('>'))
            handled = selectBlockTextObject(g.subsubdata.is('i'), '<', '>');
        else if (input.is('{') || input.is('}') || input.is('B'))
            handled = selectBlockTextObject(g.subsubdata.is('i'), '{', '}');
        else if (input.is('"') || input.is('\'') || input.is('`'))
            handled = selectQuotedStringTextObject(g.subsubdata.is('i'), input.asChar());
        else
            handled = false;
        g.subsubmode = NoSubSubMode;
        if (handled) {
            finishMovement(QString("%1%2%3")
                           .arg(count())
                           .arg(g.subsubdata.text())
                           .arg(input.text()));
        }
    } else if (g.subsubmode == MarkSubSubMode) {
        setMark(input.asChar(), CursorPosition(m_cursor));
        g.subsubmode = NoSubSubMode;
    } else if (g.subsubmode == BackTickSubSubMode
            || g.subsubmode == TickSubSubMode) {
        handled = jumpToMark(input.asChar(), g.subsubmode == BackTickSubSubMode);
        if (handled)
            finishMovement();
        g.subsubmode = NoSubSubMode;
    } else if (g.subsubmode == ZSubSubMode) {
        handled = false;
        if (input.is('j') || input.is('k')) {
            int pos = position();
            q->foldGoTo(input.is('j') ? count() : -count(), false);
            if (pos != position()) {
                handled = true;
                finishMovement(QString("%1z%2")
                               .arg(count())
                               .arg(input.text()));
            }
        }
    } else if (g.subsubmode == OpenSquareSubSubMode || g.subsubmode == CloseSquareSubSubMode) {
        int pos = position();
        if (input.is('{') && g.subsubmode == OpenSquareSubSubMode)
            searchBalanced(false, '{', '}');
        else if (input.is('}') && g.subsubmode == CloseSquareSubSubMode)
            searchBalanced(true, '}', '{');
        else if (input.is('(') && g.subsubmode == OpenSquareSubSubMode)
            searchBalanced(false, '(', ')');
        else if (input.is(')') && g.subsubmode == CloseSquareSubSubMode)
            searchBalanced(true, ')', '(');
        else if (input.is('[') && g.subsubmode == OpenSquareSubSubMode)
            bracketSearchBackward(&m_cursor, "^\\{", count());
        else if (input.is('[') && g.subsubmode == CloseSquareSubSubMode)
            bracketSearchForward(&m_cursor, "^\\}", count(), false);
        else if (input.is(']') && g.subsubmode == OpenSquareSubSubMode)
            bracketSearchBackward(&m_cursor, "^\\}", count());
        else if (input.is(']') && g.subsubmode == CloseSquareSubSubMode)
            bracketSearchForward(&m_cursor, "^\\{", count(), g.submode != NoSubMode);
        else if (input.is('z'))
            q->foldGoTo(g.subsubmode == OpenSquareSubSubMode ? -count() : count(), true);
        handled = pos != position();
        if (handled) {
            if (lineForPosition(pos) != lineForPosition(position()))
                recordJump(pos);
            finishMovement(QString("%1%2%3")
                           .arg(count())
                           .arg(g.subsubmode == OpenSquareSubSubMode ? '[' : ']')
                           .arg(input.text()));
        }
    } else {
        handled = false;
    }
    return handled;
}

bool FakeVimHandler::Private::handleCount(const Input &input)
{
    if (!isInputCount(input))
        return false;
    g.mvcount = g.mvcount * 10 + input.text().toInt();
    return true;
}

bool FakeVimHandler::Private::handleMovement(const Input &input)
{
    bool handled = true;
    int count = this->count();

    if (handleCount(input)) {
        return true;
    } else if (input.is('0')) {
        g.movetype = MoveExclusive;
        if (g.gflag)
            moveToStartOfLineVisually();
        else
            moveToStartOfLine();
        count = 1;
    } else if (input.is('a') || input.is('i')) {
        g.subsubmode = TextObjectSubSubMode;
        g.subsubdata = input;
    } else if (input.is('^') || input.is('_')) {
        if (g.gflag)
            moveToFirstNonBlankOnLineVisually();
        else
            moveToFirstNonBlankOnLine();
        g.movetype = MoveExclusive;
    } else if (0 && input.is(',')) {
        // FIXME: fakevim uses ',' by itself, so it is incompatible
        g.subsubmode = FtSubSubMode;
        // HACK: toggle 'f' <-> 'F', 't' <-> 'T'
        //g.subsubdata = g.semicolonType ^ 32;
        handleFfTt(g.semicolonKey, true);
        g.subsubmode = NoSubSubMode;
    } else if (input.is(';')) {
        g.subsubmode = FtSubSubMode;
        g.subsubdata = g.semicolonType;
        handleFfTt(g.semicolonKey, true);
        g.subsubmode = NoSubSubMode;
    } else if (input.is('/') || input.is('?')) {
        g.lastSearchForward = input.is('/');
        if (hasConfig(ConfigUseCoreSearch)) {
            // re-use the core dialog.
            g.findPending = true;
            m_findStartPosition = position();
            g.movetype = MoveExclusive;
            setAnchor(); // clear selection: otherwise, search is restricted to selection
            q->findRequested(!g.lastSearchForward);
        } else {
            // FIXME: make core find dialog sufficiently flexible to
            // produce the "default vi" behaviour too. For now, roll our own.
            g.currentMessage.clear();
            g.movetype = MoveExclusive;
            g.subsubmode = SearchSubSubMode;
            g.searchBuffer.setPrompt(g.lastSearchForward ? '/' : '?');
            m_searchStartPosition = position();
            m_searchFromScreenLine = firstVisibleLine();
            m_searchCursor = QTextCursor();
            g.searchBuffer.clear();
        }
    } else if (input.is('`')) {
        g.subsubmode = BackTickSubSubMode;
    } else if (input.is('#') || input.is('*')) {
        // FIXME: That's not proper vim behaviour
        QString needle;
        QTextCursor tc = m_cursor;
        tc.select(QTextCursor::WordUnderCursor);
        needle = QRegularExpression::escape(tc.selection().toPlainText());
        if (!g.gflag) {
            needle.prepend("\\<");
            needle.append("\\>");
        }
        setAnchorAndPosition(tc.position(), tc.anchor());
        g.searchBuffer.historyPush(needle);
        g.lastSearch = needle;
        g.lastSearchForward = input.is('*');
        handled = searchNext();
    } else if (input.is('\'')) {
        g.subsubmode = TickSubSubMode;
        if (g.submode != NoSubMode)
            g.movetype = MoveLineWise;
    } else if (input.is('|')) {
        moveToStartOfLine();
        const int column = count - 1;
        moveRight(qMin(column, rightDist() - 1));
        m_targetColumn = column;
        m_visualTargetColumn = column;
    } else if (input.is('{') || input.is('}')) {
        const int oldPosition = position();
        handled = input.is('}')
            ? moveToNextParagraph(count)
            : moveToPreviousParagraph(count);
        if (handled) {
            recordJump(oldPosition);
            setTargetColumn();
            g.movetype = MoveExclusive;
        }
    } else if (input.isReturn()) {
        moveToStartOfLine();
        moveDown();
        moveToFirstNonBlankOnLine();
    } else if (input.is('-')) {
        moveToStartOfLine();
        moveUp(count);
        moveToFirstNonBlankOnLine();
    } else if (input.is('+')) {
        moveToStartOfLine();
        moveDown(count);
        moveToFirstNonBlankOnLine();
    } else if (input.isKey(Key_Home)) {
        moveToStartOfLine();
    } else if (input.is('$') || input.isKey(Key_End)) {
        if (g.gflag) {
            if (count > 1)
                moveDownVisually(count - 1);
            moveToEndOfLineVisually();
        } else {
            if (count > 1)
                moveDown(count - 1);
            moveToEndOfLine();
        }
        g.movetype = atEmptyLine() ? MoveExclusive : MoveInclusive;
        if (g.submode == NoSubMode)
            m_targetColumn = -1;
        if (isVisualMode())
            m_visualTargetColumn = -1;
    } else if (input.is('%')) {
        recordJump();
        if (g.mvcount == 0) {
            moveToMatchingParanthesis();
            g.movetype = MoveInclusive;
        } else {
            // set cursor position in percentage - formula taken from Vim help
            setPosition(firstPositionInLine((count * linesInDocument() + 99) / 100));
            moveToTargetColumn();
            handleStartOfLine();
            g.movetype = MoveLineWise;
        }
    } else if (input.is('b') || input.isShift(Key_Left)) {
        moveToNextWordStart(count, false, false);
    } else if (input.is('B') || input.isControl(Key_Left)) {
        moveToNextWordStart(count, true, false);
    } else if (input.is('e') && g.gflag) {
        moveToNextWordEnd(count, false, false);
    } else if (input.is('e')) {
        moveToNextWordEnd(count, false, true, false);
    } else if (input.is('E') && g.gflag) {
        moveToNextWordEnd(count, true, false);
    } else if (input.is('E')) {
        moveToNextWordEnd(count, true, true, false);
    } else if (input.isControl('e')) {
        // FIXME: this should use the "scroll" option, and "count"
        if (cursorLineOnScreen() == 0)
            moveDown(1);
        scrollDown(1);
    } else if (input.is('f')) {
        g.subsubmode = FtSubSubMode;
        g.movetype = MoveInclusive;
        g.subsubdata = input;
    } else if (input.is('F')) {
        g.subsubmode = FtSubSubMode;
        g.movetype = MoveExclusive;
        g.subsubdata = input;
    } else if (!g.gflag && input.is('g')) {
        g.gflag = true;
        return true;
    } else if (input.is('g') || input.is('G')) {
        QString dotCommand = QString("%1G").arg(count);
        recordJump();
        if (input.is('G') && g.mvcount == 0)
            dotCommand = "G";
        int n = (input.is('g')) ? 1 : linesInDocument();
        n = g.mvcount == 0 ? n : count;
        if (g.submode == NoSubMode || g.submode == ZSubMode
                || g.submode == CapitalZSubMode || g.submode == RegisterSubMode) {
            setPosition(firstPositionInLine(n, false));
            handleStartOfLine();
        } else {
            g.movetype = MoveLineWise;
            g.rangemode = RangeLineMode;
            setAnchor();
            setPosition(firstPositionInLine(n, false));
        }
        setTargetColumn();
        updateScrollOffset();
    } else if (input.is('h') || input.isKey(Key_Left) || input.isBackspace()) {
        g.movetype = MoveExclusive;
        int n = qMin(count, leftDist());
        moveLeft(n);
    } else if (input.is('H')) {
        const CursorPosition pos(lineToBlockNumber(lineOnTop(count)), 0);
        setCursorPosition(&m_cursor, pos);
        handleStartOfLine();
    } else if (input.is('j') || input.isKey(Key_Down)
            || input.isControl('j') || input.isControl('n')) {
        moveVertically(count);
    } else if (input.is('k') || input.isKey(Key_Up) || input.isControl('p')) {
        moveVertically(-count);
    } else if (input.is('l') || input.isKey(Key_Right) || input.is(' ')) {
        g.movetype = MoveExclusive;
        moveRight(qMax(0, qMin(count, rightDist() - (g.submode == NoSubMode))));
    } else if (input.is('L')) {
        const CursorPosition pos(lineToBlockNumber(lineOnBottom(count)), 0);
        setCursorPosition(&m_cursor, pos);
        handleStartOfLine();
    } else if (g.gflag && input.is('m')) {
        const QPoint pos(EDITOR(viewport()->width()) / 2, EDITOR(cursorRect(m_cursor)).y());
        QTextCursor tc = EDITOR(cursorForPosition(pos));
        if (!tc.isNull()) {
            m_cursor = tc;
            setTargetColumn();
        }
    } else if (input.is('M')) {
        m_cursor = EDITOR(cursorForPosition(QPoint(0, EDITOR(height()) / 2)));
        handleStartOfLine();
    } else if (input.is('n') || input.is('N')) {
        if (hasConfig(ConfigUseCoreSearch)) {
            bool forward = (input.is('n')) ? g.lastSearchForward : !g.lastSearchForward;
            int pos = position();
            q->findNextRequested(!forward);
            if (forward && pos == m_cursor.selectionStart()) {
                // if cursor is already positioned at the start of a find result, this is returned
                q->findNextRequested(false);
            }
            setPosition(m_cursor.selectionStart());
        } else {
            handled = searchNext(input.is('n'));
        }
    } else if (input.is('t')) {
        g.movetype = MoveInclusive;
        g.subsubmode = FtSubSubMode;
        g.subsubdata = input;
    } else if (input.is('T')) {
        g.movetype = MoveExclusive;
        g.subsubmode = FtSubSubMode;
        g.subsubdata = input;
    } else if (input.is('w') || input.is('W')
            || input.isShift(Key_Right) || input.isControl(Key_Right)) { // tested
        // Special case: "cw" and "cW" work the same as "ce" and "cE" if the
        // cursor is on a non-blank - except if the cursor is on the last
        // character of a word: only the current word will be changed
        bool simple = input.is('W') || input.isControl(Key_Right);
        if (g.submode == ChangeSubMode && !characterAtCursor().isSpace()) {
            moveToWordEnd(count, simple, true);
        } else {
            moveToNextWordStart(count, simple, true);
            // Command 'dw' deletes to the next word on the same line or to end of line.
            if (g.submode == DeleteSubMode && count == 1) {
                const QTextBlock currentBlock = blockAt(anchor());
                setPosition(qMin(position(), currentBlock.position() + currentBlock.length()));
            }
        }
    } else if (input.is('z')) {
        g.movetype =  MoveLineWise;
        g.subsubmode = ZSubSubMode;
    } else if (input.is('[')) {
        g.subsubmode = OpenSquareSubSubMode;
    } else if (input.is(']')) {
        g.subsubmode = CloseSquareSubSubMode;
    } else if (input.isKey(Key_PageDown) || input.isControl('f')) {
        movePageDown(count);
        handleStartOfLine();
    } else if (input.isKey(Key_PageUp) || input.isControl('b')) {
        movePageUp(count);
        handleStartOfLine();
    } else {
        handled = false;
    }

    if (handled && g.subsubmode == NoSubSubMode) {
        if (g.submode == NoSubMode) {
            leaveCurrentMode();
        } else {
            // finish movement for sub modes
            const QString dotMovement =
                (count > 1 ? QString::number(count) : QString())
                + QLatin1String(g.gflag ? "g" : "")
                + input.toString();
            finishMovement(dotMovement);
            setTargetColumn();
        }
    }

    return handled;
}

EventResult FakeVimHandler::Private::handleCommandMode(const Input &input)
{
    bool handled = false;

    bool clearGflag = g.gflag;
    bool clearRegister = g.submode != RegisterSubMode;
    bool clearCount = g.submode != RegisterSubMode && !isInputCount(input);

    // Process input for a sub-mode.
    if (input.isEscape()) {
        handled = handleEscape();
    } else if (m_wasReadOnly) {
        return EventUnhandled;
    } else if (g.subsubmode != NoSubSubMode) {
        handled = handleCommandSubSubMode(input);
    } else if (g.submode == NoSubMode) {
        handled = handleNoSubMode(input);
    } else if (g.submode == ChangeSubMode
        || g.submode == DeleteSubMode
        || g.submode == YankSubMode) {
        handled = handleChangeDeleteYankSubModes(input);
    } else if (g.submode == ReplaceSubMode) {
        handled = handleReplaceSubMode(input);
    } else if (g.submode == FilterSubMode) {
        handled = handleFilterSubMode(input);
    } else if (g.submode == RegisterSubMode) {
        handled = handleRegisterSubMode(input);
    } else if (g.submode == WindowSubMode) {
        handled = handleWindowSubMode(input);
    } else if (g.submode == ZSubMode) {
        handled = handleZSubMode(input);
    } else if (g.submode == CapitalZSubMode) {
        handled = handleCapitalZSubMode(input);
    } else if (g.submode == MacroRecordSubMode) {
        handled = handleMacroRecordSubMode(input);
    } else if (g.submode == MacroExecuteSubMode) {
        handled = handleMacroExecuteSubMode(input);
    } else if (g.submode == ShiftLeftSubMode
        || g.submode == ShiftRightSubMode
        || g.submode == IndentSubMode) {
        handled = handleShiftSubMode(input);
    } else if (g.submode == InvertCaseSubMode
        || g.submode == DownCaseSubMode
        || g.submode == UpCaseSubMode) {
        handled = handleChangeCaseSubMode(input);
    }

    if (!handled && isOperatorPending())
       handled = handleMovement(input);

    // Clear state and display incomplete command if necessary.
    if (handled) {
        bool noMode =
            (g.mode == CommandMode && g.submode == NoSubMode && g.subsubmode == NoSubSubMode);
        clearCount = clearCount && noMode && !g.gflag;
        if (clearCount && clearRegister) {
            leaveCurrentMode();
        } else {
            // Use gflag only for next input.
            if (clearGflag)
                g.gflag = false;
            // Clear [count] and [register] if its no longer needed.
            if (clearCount)
                resetCount();
            // Show or clear current command on minibuffer (showcmd).
            if (input.isEscape() || g.mode != CommandMode || clearCount)
                g.currentCommand.clear();
            else
                g.currentCommand.append(input.toString());
        }

        saveLastVisualMode();
    } else {
        leaveCurrentMode();
        //qDebug() << "IGNORED IN COMMAND MODE: " << key << text
        //    << " VISUAL: " << g.visualMode;

        // if a key which produces text was pressed, don't mark it as unhandled
        // - otherwise the text would be inserted while being in command mode
        if (input.text().isEmpty())
            handled = false;
    }

    m_positionPastEnd = (m_visualTargetColumn == -1) && isVisualMode() && !atEmptyLine();

    return handled ? EventHandled : EventCancelled;
}

bool FakeVimHandler::Private::handleEscape()
{
    if (isVisualMode())
        leaveVisualMode();
    leaveCurrentMode();
    return true;
}

bool FakeVimHandler::Private::handleNoSubMode(const Input &input)
{
    bool handled = true;

    const int oldRevision = revision();
    QString dotCommand = visualDotCommand()
            + QLatin1String(g.gflag ? "g" : "")
            + QString::number(count())
            + input.toString();

    if (input.is('&')) {
        handleExCommand(QLatin1String(g.gflag ? "%s//~/&" : "s"));
    } else if (input.is(':')) {
        enterExMode();
    } else if (input.is('!') && isNoVisualMode()) {
        g.submode = FilterSubMode;
    } else if (input.is('!') && isVisualMode()) {
        enterExMode(QString("!"));
    } else if (input.is('"')) {
        g.submode = RegisterSubMode;
    } else if (input.is(',')) {
        passShortcuts(true);
    } else if (input.is('.')) {
        //qDebug() << "REPEATING" << quoteUnprintable(g.dotCommand) << count()
        //    << input;
        dotCommand.clear();
        QString savedCommand = g.dotCommand;
        g.dotCommand.clear();
        beginLargeEditBlock();
        replay(savedCommand);
        endEditBlock();
        leaveCurrentMode();
        g.dotCommand = savedCommand;
    } else if (input.is('<') || input.is('>') || input.is('=')) {
        g.submode = indentModeFromInput(input);
        if (isVisualMode()) {
            leaveVisualMode();
            const int repeat = count();
            if (g.submode == ShiftLeftSubMode)
                shiftRegionLeft(repeat);
            else if (g.submode == ShiftRightSubMode)
                shiftRegionRight(repeat);
            else
                indentSelectedText();
            g.submode = NoSubMode;
        } else {
            setAnchor();
        }
    } else if ((!isVisualMode() && input.is('a')) || (isVisualMode() && input.is('A'))) {
        if (isVisualMode()) {
            if (!isVisualBlockMode())
                dotCommand = QString::number(count()) + "a";
            enterVisualInsertMode('A');
        } else {
            moveRight(qMin(rightDist(), 1));
            breakEditBlock();
            enterInsertMode();
        }
    } else if (input.is('A')) {
        breakEditBlock();
        moveBehindEndOfLine();
        setAnchor();
        enterInsertMode();
        setTargetColumn();
    } else if (input.isControl('a')) {
        changeNumberTextObject(count());
    } else if ((input.is('c') || input.is('d') || input.is('y')) && isNoVisualMode()) {
        setAnchor();
        g.opcount = g.mvcount;
        g.mvcount = 0;
        g.rangemode = RangeCharMode;
        g.movetype = MoveExclusive;
        g.submode = changeDeleteYankModeFromInput(input);
    } else if ((input.is('c') || input.is('C') || input.is('s') || input.is('R'))
          && (isVisualCharMode() || isVisualLineMode())) {
        leaveVisualMode();
        g.submode = ChangeSubMode;
        finishMovement();
    } else if ((input.is('c') || input.is('s')) && isVisualBlockMode()) {
        resetCount();
        enterVisualInsertMode(input.asChar());
    } else if (input.is('C')) {
        handleAs("%1c$");
    } else if (input.isControl('c')) {
        if (isNoVisualMode())
            showMessage(MessageInfo, Tr::tr("Type Alt-V, Alt-V to quit FakeVim mode."));
        else
            leaveVisualMode();
    } else if ((input.is('d') || input.is('x') || input.isKey(Key_Delete))
            && isVisualMode()) {
        cutSelectedText();
    } else if (input.is('D') && isNoVisualMode()) {
        handleAs("%1d$");
    } else if ((input.is('D') || input.is('X')) && isVisualMode()) {
        if (isVisualCharMode())
            toggleVisualMode(VisualLineMode);
        if (isVisualBlockMode() && input.is('D'))
            m_visualTargetColumn = -1;
        cutSelectedText();
    } else if (input.isControl('d')) {
        const int scrollOffset = windowScrollOffset();
        int sline = cursorLine() < scrollOffset ? scrollOffset : cursorLineOnScreen();
        // FIXME: this should use the "scroll" option, and "count"
        moveDown(linesOnScreen() / 2);
        handleStartOfLine();
        scrollToLine(cursorLine() - sline);
    } else if (!g.gflag && input.is('g')) {
        g.gflag = true;
    } else if (!isVisualMode() && (input.is('i') || input.isKey(Key_Insert))) {
        breakEditBlock();
        enterInsertMode();
        if (atEndOfLine())
            moveLeft();
    } else if (input.is('I')) {
        if (isVisualMode()) {
            if (!isVisualBlockMode())
                dotCommand = QString::number(count()) + "i";
            enterVisualInsertMode('I');
        } else {
            if (g.gflag)
                moveToStartOfLine();
            else
                moveToFirstNonBlankOnLine();
            breakEditBlock();
            enterInsertMode();
        }
    } else if (input.isControl('i')) {
        jump(count());
    } else if (input.is('J')) {
        pushUndoState();
        moveBehindEndOfLine();
        beginEditBlock();
        if (g.submode == NoSubMode)
            joinLines(count(), g.gflag);
        endEditBlock();
    } else if (input.isControl('l')) {
        // screen redraw. should not be needed
    } else if (!g.gflag && input.is('m')) {
        g.subsubmode = MarkSubSubMode;
    } else if (isVisualMode() && (input.is('o') || input.is('O'))) {
        int pos = position();
        setAnchorAndPosition(pos, anchor());
        std::swap(m_positionPastEnd, m_anchorPastEnd);
        setTargetColumn();
        if (m_positionPastEnd)
            m_visualTargetColumn = -1;
    } else if (input.is('o') || input.is('O')) {
        bool insertAfter = input.is('o');
        pushUndoState();

        // Prepend line only if on the first line and command is 'O'.
        bool appendLine = true;
        if (!insertAfter) {
            if (block().blockNumber() == 0)
                appendLine = false;
            else
                moveUp();
        }
        const int line = lineNumber(block());

        beginEditBlock();
        enterInsertMode();
        setPosition(appendLine ? lastPositionInLine(line) : firstPositionInLine(line));
        clearLastInsertion();
        setAnchor();
        insertNewLine();
        if (appendLine) {
            m_buffer->insertState.newLineBefore = true;
        } else {
            moveUp();
            m_buffer->insertState.pos1 = position();
            m_buffer->insertState.newLineAfter = true;
        }
        setTargetColumn();
        endEditBlock();

        // Close accidentally opened block.
        if (block().blockNumber() > 0) {
            moveUp();
            if (line != lineNumber(block()))
                q->fold(1, true);
            moveDown();
        }
    } else if (input.isControl('o')) {
        jump(-count());
    } else if (input.is('p') || input.is('P') || input.isShift(Qt::Key_Insert)) {
        pasteText(!input.is('P'));
        setTargetColumn();
        finishMovement();
    } else if (input.is('q')) {
        if (g.isRecording) {
            // Stop recording.
            stopRecording();
        } else {
            // Recording shouldn't work in mapping or while executing register.
            handled = g.mapStates.empty();
            if (handled)
                g.submode = MacroRecordSubMode;
        }
    } else if (input.is('r')) {
        g.submode = ReplaceSubMode;
    } else if (!isVisualMode() && input.is('R')) {
        pushUndoState();
        breakEditBlock();
        enterReplaceMode();
    } else if (input.isControl('r')) {
        dotCommand.clear();
        int repeat = count();
        while (--repeat >= 0)
            redo();
    } else if (input.is('s')) {
        handleAs("c%1l");
    } else if (input.is('S')) {
        handleAs("%1cc");
    } else if (g.gflag && input.is('t')) {
        handleExCommand("tabnext");
    } else if (g.gflag && input.is('T')) {
        handleExCommand("tabprevious");
    } else if (input.isControl('t')) {
        handleExCommand("pop");
    } else if (!g.gflag && input.is('u') && !isVisualMode()) {
        dotCommand.clear();
        int repeat = count();
        while (--repeat >= 0)
            undo();
    } else if (input.isControl('u')) {
        int sline = cursorLineOnScreen();
        // FIXME: this should use the "scroll" option, and "count"
        moveUp(linesOnScreen() / 2);
        handleStartOfLine();
        scrollToLine(cursorLine() - sline);
    } else if (g.gflag && input.is('v')) {
        if (isNoVisualMode()) {
            CursorPosition from = markLessPosition();
            CursorPosition to = markGreaterPosition();
            if (m_buffer->lastVisualModeInverted)
                std::swap(from, to);
            toggleVisualMode(m_buffer->lastVisualMode);
            setCursorPosition(from);
            setAnchor();
            setCursorPosition(to);
            setTargetColumn();
        }
    } else if (input.is('v')) {
        toggleVisualMode(VisualCharMode);
    } else if (input.is('V')) {
        toggleVisualMode(VisualLineMode);
    } else if (input.isControl('v')) {
        toggleVisualMode(VisualBlockMode);
    } else if (input.isControl('w')) {
        g.submode = WindowSubMode;
    } else if (input.is('x') && isNoVisualMode()) {
        handleAs("%1dl");
    } else if (input.isControl('x')) {
        changeNumberTextObject(-count());
    } else if (input.is('X')) {
        handleAs("%1dh");
    } else if (input.is('Y') && isNoVisualMode())  {
        handleAs("%1yy");
    } else if (input.isControl('y')) {
        // FIXME: this should use the "scroll" option, and "count"
        if (cursorLineOnScreen() == linesOnScreen() - 1)
            moveUp(1);
        scrollUp(1);
    } else if (input.is('y') && isVisualCharMode()) {
        g.rangemode = RangeCharMode;
        g.movetype = MoveInclusive;
        g.submode = YankSubMode;
        finishMovement();
    } else if ((input.is('y') && isVisualLineMode())
            || (input.is('Y') && isVisualLineMode())
            || (input.is('Y') && isVisualCharMode())) {
        g.rangemode = RangeLineMode;
        g.movetype = MoveLineWise;
        g.submode = YankSubMode;
        finishMovement();
    } else if ((input.is('y') || input.is('Y')) && isVisualBlockMode()) {
        g.rangemode = RangeBlockMode;
        g.movetype = MoveInclusive;
        g.submode = YankSubMode;
        finishMovement();
    } else if (input.is('z')) {
        g.submode = ZSubMode;
    } else if (input.is('Z')) {
        g.submode = CapitalZSubMode;
    } else if ((input.is('~') || input.is('u') || input.is('U'))) {
        g.movetype = MoveExclusive;
        g.submode = letterCaseModeFromInput(input);
        pushUndoState();
        if (isVisualMode()) {
            leaveVisualMode();
            finishMovement();
        } else if (g.gflag || (g.submode == InvertCaseSubMode && hasConfig(ConfigTildeOp))) {
            if (atEndOfLine())
                moveLeft();
            setAnchor();
        } else {
            const QString movementCommand = QString("%1l%1l").arg(count());
            handleAs("g" + input.toString() + movementCommand);
        }
    } else if (input.is('@')) {
        g.submode = MacroExecuteSubMode;
    } else if (input.isKey(Key_Delete)) {
        setAnchor();
        moveRight(qMin(1, rightDist()));
        removeText(currentRange());
        if (atEndOfLine())
            moveLeft();
    } else if (input.isControl(Key_BracketRight)) {
        handleExCommand("tag");
    } else if (handleMovement(input)) {
        // movement handled
        dotCommand.clear();
    } else {
        handled = false;
    }

    // Set dot command if the current input changed document or entered insert mode.
    if (handled && !dotCommand.isEmpty() && (oldRevision != revision() || isInsertMode()))
        setDotCommand(dotCommand);

    return handled;
}

bool FakeVimHandler::Private::handleChangeDeleteYankSubModes(const Input &input)
{
    if (g.submode != changeDeleteYankModeFromInput(input))
        return false;

    handleChangeDeleteYankSubModes();

    return true;
}

void FakeVimHandler::Private::handleChangeDeleteYankSubModes()
{
    g.movetype = MoveLineWise;

    const QString dotCommand = dotCommandFromSubMode(g.submode);

    if (!dotCommand.isEmpty())
        pushUndoState();

    const int anc = firstPositionInLine(cursorLine() + 1);
    moveDown(count() - 1);
    const int pos = lastPositionInLine(cursorLine() + 1);
    setAnchorAndPosition(anc, pos);

    if (!dotCommand.isEmpty())
        setDotCommand(QString("%2%1%1").arg(dotCommand), count());

    finishMovement();

    g.submode = NoSubMode;
}

bool FakeVimHandler::Private::handleReplaceSubMode(const Input &input)
{
    bool handled = true;

    const QChar c = input.asChar();
    setDotCommand(visualDotCommand() + 'r' + c);
    if (isVisualMode()) {
        pushUndoState();
        leaveVisualMode();
        Range range = currentRange();
        if (g.rangemode == RangeCharMode)
            ++range.endPos;
        // Replace each character but preserve lines.
        transformText(range, [&c](const QString &text) {
            return QString(text).replace(QRegularExpression("[^\\n]"), c);
        });
    } else if (count() <= rightDist()) {
        pushUndoState();
        setAnchor();
        moveRight(count());
        Range range = currentRange();
        if (input.isReturn()) {
            beginEditBlock();
            replaceText(range, QString());
            insertText(QString("\n"));
            endEditBlock();
        } else {
            replaceText(range, QString(count(), c));
            moveRight(count() - 1);
        }
        setTargetColumn();
        setDotCommand("%1r" + input.text(), count());
    } else {
        handled = false;
    }
    g.submode = NoSubMode;
    finishMovement();

    return handled;
}

bool FakeVimHandler::Private::handleFilterSubMode(const Input &)
{
    return false;
}

bool FakeVimHandler::Private::handleRegisterSubMode(const Input &input)
{
    bool handled = false;

    QChar reg = input.asChar();
    if (QString("*+.%#:-\"_").contains(reg) || reg.isLetterOrNumber()) {
        m_register = reg.unicode();
        handled = true;
    }
    g.submode = NoSubMode;

    return handled;
}

bool FakeVimHandler::Private::handleShiftSubMode(const Input &input)
{
    if (g.submode != indentModeFromInput(input))
        return false;

    g.movetype = MoveLineWise;
    pushUndoState();
    moveDown(count() - 1);
    setDotCommand(QString("%2%1%1").arg(input.asChar()), count());
    finishMovement();
    g.submode = NoSubMode;

    return true;
}

bool FakeVimHandler::Private::handleChangeCaseSubMode(const Input &input)
{
    if (g.submode != letterCaseModeFromInput(input))
        return false;

    if (!isFirstNonBlankOnLine(position())) {
        moveToStartOfLine();
        moveToFirstNonBlankOnLine();
    }
    setTargetColumn();
    pushUndoState();
    setAnchor();
    setPosition(lastPositionInLine(cursorLine() + count()) + 1);
    finishMovement(QString("%1%2").arg(count()).arg(input.raw()));
    g.submode = NoSubMode;

    return true;
}

bool FakeVimHandler::Private::handleWindowSubMode(const Input &input)
{
    if (handleCount(input))
        return true;

    leaveVisualMode();
    leaveCurrentMode();
    q->windowCommandRequested(input.toString(), count());

    return true;
}

bool FakeVimHandler::Private::handleZSubMode(const Input &input)
{
    bool handled = true;
    bool foldMaybeClosed = false;
    if (input.isReturn() || input.is('t')
        || input.is('-') || input.is('b')
        || input.is('.') || input.is('z')) {
        // Cursor line to top/center/bottom of window.
        Qt::AlignmentFlag align;
        if (input.isReturn() || input.is('t'))
            align = Qt::AlignTop;
        else if (input.is('.') || input.is('z'))
            align = Qt::AlignVCenter;
        else
            align = Qt::AlignBottom;
        const bool moveToNonBlank = (input.is('.') || input.isReturn() || input.is('-'));
        const int line = g.mvcount == 0 ? -1 : firstPositionInLine(count());
        alignViewportToCursor(align, line, moveToNonBlank);
    } else if (input.is('o') || input.is('c')) {
        // Open/close current fold.
        foldMaybeClosed = input.is('c');
        q->fold(count(), foldMaybeClosed);
    } else if (input.is('O') || input.is('C')) {
        // Recursively open/close current fold.
        foldMaybeClosed = input.is('C');
        q->fold(-1, foldMaybeClosed);
    } else if (input.is('a') || input.is('A')) {
        // Toggle current fold.
        foldMaybeClosed = true;
        q->foldToggle(input.is('a') ? count() : -1);
    } else if (input.is('R') || input.is('M')) {
        // Open/close all folds in document.
        foldMaybeClosed = input.is('M');
        q->foldAll(foldMaybeClosed);
    } else if (input.is('j') || input.is('k')) {
        q->foldGoTo(input.is('j') ? count() : -count(), false);
    } else {
        handled = false;
    }
    if (foldMaybeClosed)
        ensureCursorVisible();
    g.submode = NoSubMode;
    return handled;
}

bool FakeVimHandler::Private::handleCapitalZSubMode(const Input &input)
{
    // Recognize ZZ and ZQ as aliases for ":x" and ":q!".
    bool handled = true;
    if (input.is('Z'))
        handleExCommand("x");
    else if (input.is('Q'))
        handleExCommand("q!");
    else
        handled = false;
    g.submode = NoSubMode;
    return handled;
}

bool FakeVimHandler::Private::handleMacroRecordSubMode(const Input &input)
{
    g.submode = NoSubMode;
    return startRecording(input);
}

bool FakeVimHandler::Private::handleMacroExecuteSubMode(const Input &input)
{
    g.submode = NoSubMode;

    bool result = true;
    int repeat = count();
    while (result && --repeat >= 0)
        result = executeRegister(input.asChar().unicode());

    return result;
}

EventResult FakeVimHandler::Private::handleInsertOrReplaceMode(const Input &input)
{
    if (position() < m_buffer->insertState.pos1 || position() > m_buffer->insertState.pos2) {
        commitInsertState();
        invalidateInsertState();
    }

    if (g.mode == InsertMode)
        handleInsertMode(input);
    else
        handleReplaceMode(input);

    if (!m_textedit && !m_plaintextedit)
        return EventHandled;

    if (!isInsertMode() || m_buffer->breakEditBlock
            || position() < m_buffer->insertState.pos1 || position() > m_buffer->insertState.pos2) {
        commitInsertState();
        invalidateInsertState();
        breakEditBlock();
        m_visualBlockInsert = NoneBlockInsertMode;
    }

    // We don't want fancy stuff in insert mode.
    return EventHandled;
}

void FakeVimHandler::Private::handleReplaceMode(const Input &input)
{
    if (input.isEscape()) {
        commitInsertState();
        moveLeft(qMin(1, leftDist()));
        enterCommandMode();
        g.dotCommand.append(m_buffer->lastInsertion + "<ESC>");
    } else if (input.isKey(Key_Left)) {
        moveLeft();
    } else if (input.isKey(Key_Right)) {
        moveRight();
    } else if (input.isKey(Key_Up)) {
        moveUp();
    } else if (input.isKey(Key_Down)) {
        moveDown();
    } else if (input.isKey(Key_Insert)) {
        g.mode = InsertMode;
    } else if (input.isControl('o')) {
        enterCommandMode(ReplaceMode);
    } else {
        joinPreviousEditBlock();
        if (!atEndOfLine()) {
            setAnchor();
            moveRight();
            removeText(currentRange());
        }
        const QString text = input.text();
        setAnchor();
        insertText(text);
        setTargetColumn();
        endEditBlock();
    }
}

void FakeVimHandler::Private::finishInsertMode()
{
    bool newLineAfter = m_buffer->insertState.newLineAfter;
    bool newLineBefore = m_buffer->insertState.newLineBefore;

    // Repeat insertion [count] times.
    // One instance was already physically inserted while typing.
    if (!m_buffer->breakEditBlock && isInsertStateValid()) {
        commitInsertState();

        QString text = m_buffer->lastInsertion;
        const QString dotCommand = g.dotCommand;
        const int repeat = count() - 1;
        m_buffer->lastInsertion.clear();
        joinPreviousEditBlock();

        if (newLineAfter) {
            text.chop(1);
            text.prepend("<END>\n");
        } else if (newLineBefore) {
            text.prepend("<END>");
        }

        replay(text, repeat);

        if (m_visualBlockInsert != NoneBlockInsertMode && !text.contains('\n')) {
            const CursorPosition lastAnchor = markLessPosition();
            const CursorPosition lastPosition = markGreaterPosition();
            const bool change = m_visualBlockInsert == ChangeBlockInsertMode;
            const int insertColumn = (m_visualBlockInsert == InsertBlockInsertMode || change)
                    ? qMin(lastPosition.column, lastAnchor.column)
                    : qMax(lastPosition.column, lastAnchor.column) + 1;

            CursorPosition pos(lastAnchor.line, insertColumn);

            if (change)
                pos.column = columnAt(m_buffer->insertState.pos1);

            // Cursor position after block insert is on the first selected line,
            // last selected column for 's' command, otherwise first selected column.
            const int endColumn = change ? qMax(0, m_cursor.positionInBlock() - 1)
                                         : qMin(lastPosition.column, lastAnchor.column);

            while (pos.line < lastPosition.line) {
                ++pos.line;
                setCursorPosition(&m_cursor, pos);
                if (m_visualBlockInsert == AppendToEndOfLineBlockInsertMode) {
                    moveToEndOfLine();
                } else if (m_visualBlockInsert == AppendBlockInsertMode) {
                    // Prepend spaces if necessary.
                    int spaces = pos.column - m_cursor.positionInBlock();
                    if (spaces > 0) {
                        setAnchor();
                        m_cursor.insertText(QString(" ").repeated(spaces));
                    }
                } else if (m_cursor.positionInBlock() != pos.column) {
                    continue;
                }
                replay(text, repeat + 1);
            }

            setCursorPosition(CursorPosition(lastAnchor.line, endColumn));
        } else {
            moveLeft(qMin(1, leftDist()));
        }

        endEditBlock();
        breakEditBlock();

        m_buffer->lastInsertion = text;
        g.dotCommand = dotCommand;
    } else {
        moveLeft(qMin(1, leftDist()));
    }

    if (newLineBefore || newLineAfter)
        m_buffer->lastInsertion.remove(0, m_buffer->lastInsertion.indexOf('\n') + 1);
    g.dotCommand.append(m_buffer->lastInsertion + "<ESC>");

    setTargetColumn();
    enterCommandMode();
}

void FakeVimHandler::Private::handleInsertMode(const Input &input)
{
    if (input.isEscape()) {
        if (g.submode == CtrlRSubMode || g.submode == CtrlVSubMode) {
            g.submode = NoSubMode;
            g.subsubmode = NoSubSubMode;
            updateMiniBuffer();
        } else {
            finishInsertMode();
        }
    } else if (g.submode == CtrlRSubMode) {
        m_cursor.insertText(registerContents(input.asChar().unicode()));
        g.submode = NoSubMode;
    } else if (g.submode == CtrlVSubMode) {
        if (g.subsubmode == NoSubSubMode) {
            g.subsubmode = CtrlVUnicodeSubSubMode;
            m_ctrlVAccumulator = 0;
            if (input.is('x') || input.is('X')) {
                // ^VXnn or ^Vxnn with 00 <= nn <= FF
                // BMP Unicode codepoints ^Vunnnn with 0000 <= nnnn <= FFFF
                // any Unicode codepoint ^VUnnnnnnnn with 00000000 <= nnnnnnnn <= 7FFFFFFF
                // ^Vnnn with 000 <= nnn <= 255
                // ^VOnnn or ^Vonnn with 000 <= nnn <= 377
                m_ctrlVLength = 2;
                m_ctrlVBase = 16;
            } else if (input.is('O') || input.is('o')) {
                m_ctrlVLength = 3;
                m_ctrlVBase = 8;
            } else if (input.is('u')) {
                m_ctrlVLength = 4;
                m_ctrlVBase = 16;
            } else if (input.is('U')) {
                m_ctrlVLength = 8;
                m_ctrlVBase = 16;
            } else if (input.isDigit()) {
                bool ok;
                m_ctrlVAccumulator = input.toInt(&ok, 10);
                m_ctrlVLength = 2;
                m_ctrlVBase = 10;
            } else {
                insertInInsertMode(input.raw());
                g.submode = NoSubMode;
                g.subsubmode = NoSubSubMode;
            }
        } else {
            bool ok;
            int current = input.toInt(&ok, m_ctrlVBase);
            if (ok)
                m_ctrlVAccumulator = m_ctrlVAccumulator * m_ctrlVBase + current;
            --m_ctrlVLength;
            if (m_ctrlVLength == 0 || !ok) {
                QString s;
                if (QChar::requiresSurrogates(m_ctrlVAccumulator)) {
                    s.append(QChar(QChar::highSurrogate(m_ctrlVAccumulator)));
                    s.append(QChar(QChar::lowSurrogate(m_ctrlVAccumulator)));
                } else {
                    s.append(QChar(m_ctrlVAccumulator));
                }
                insertInInsertMode(s);
                g.submode = NoSubMode;
                g.subsubmode = NoSubSubMode;

                // Try again without Ctrl-V interpretation.
                if (!ok)
                    handleInsertMode(input);
            }
        }
    } else if (input.isControl('o')) {
        enterCommandMode(InsertMode);
    } else if (input.isControl('v')) {
        g.submode = CtrlVSubMode;
        g.subsubmode = NoSubSubMode;
        updateMiniBuffer();
    } else if (input.isControl('r')) {
        g.submode = CtrlRSubMode;
        g.subsubmode = NoSubSubMode;
        updateMiniBuffer();
    } else if (input.isControl('w')) {
        const int blockNumber = m_cursor.blockNumber();
        const int endPos = position();
        moveToNextWordStart(1, false, false);
        if (blockNumber != m_cursor.blockNumber())
            moveToEndOfLine();
        const int beginPos = position();
        Range range(beginPos, endPos, RangeCharMode);
        removeText(range);
    } else if (input.isControl('u')) {
        const int blockNumber = m_cursor.blockNumber();
        const int endPos = position();
        moveToStartOfLine();
        if (blockNumber != m_cursor.blockNumber())
            moveToEndOfLine();
        const int beginPos = position();
        Range range(beginPos, endPos, RangeCharMode);
        removeText(range);
    } else if (input.isKey(Key_Insert)) {
        g.mode = ReplaceMode;
    } else if (input.isKey(Key_Left)) {
        moveLeft();
    } else if (input.isShift(Key_Left) || input.isControl(Key_Left)) {
        moveToNextWordStart(1, false, false);
    } else if (input.isKey(Key_Down)) {
        g.submode = NoSubMode;
        moveDown();
    } else if (input.isKey(Key_Up)) {
        g.submode = NoSubMode;
        moveUp();
    } else if (input.isKey(Key_Right)) {
        moveRight();
    } else if (input.isShift(Key_Right) || input.isControl(Key_Right)) {
        moveToNextWordStart(1, false, true);
    } else if (input.isKey(Key_Home)) {
        moveToStartOfLine();
    } else if (input.isKey(Key_End)) {
        moveBehindEndOfLine();
        m_targetColumn = -1;
    } else if (input.isReturn() || input.isControl('j') || input.isControl('m')) {
        if (!input.isReturn() || !handleInsertInEditor(input)) {
            joinPreviousEditBlock();
            g.submode = NoSubMode;
            insertNewLine();
            endEditBlock();
        }
    } else if (input.isBackspace()) {
        // pass C-h as backspace, too
        if (!handleInsertInEditor(Input(Qt::Key_Backspace, Qt::NoModifier))) {
            joinPreviousEditBlock();
            if (!m_buffer->lastInsertion.isEmpty()
                    || hasConfig(ConfigBackspace, "start")
                    || hasConfig(ConfigBackspace, "2")) {
                const int line = cursorLine() + 1;
                const Column col = cursorColumn();
                QString data = lineContents(line);
                const Column ind = indentation(data);
                if (col.logical <= ind.logical && col.logical
                        && startsWithWhitespace(data, col.physical)) {
                    const int ts = config(ConfigTabStop).toInt();
                    const int newl = col.logical - 1 - (col.logical - 1) % ts;
                    const QString prefix = tabExpand(newl);
                    setLineContents(line, prefix + data.mid(col.physical));
                    moveToStartOfLine();
                    moveRight(prefix.size());
                } else {
                    setAnchor();
                    m_cursor.deletePreviousChar();
                }
            }
            endEditBlock();
        }
    } else if (input.isKey(Key_Delete)) {
        if (!handleInsertInEditor(input)) {
            joinPreviousEditBlock();
            m_cursor.deleteChar();
            endEditBlock();
        }
    } else if (input.isKey(Key_PageDown) || input.isControl('f')) {
        movePageDown();
    } else if (input.isKey(Key_PageUp) || input.isControl('b')) {
        movePageUp();
    } else if (input.isKey(Key_Tab)) {
        m_buffer->insertState.insertingSpaces = true;
        if (hasConfig(ConfigExpandTab)) {
            const int ts = config(ConfigTabStop).toInt();
            const int col = logicalCursorColumn();
            QString str = QString(ts - col % ts, ' ');
            insertText(str);
        } else {
            insertInInsertMode(input.raw());
        }
        m_buffer->insertState.insertingSpaces = false;
    } else if (input.isControl('d')) {
        // remove one level of indentation from the current line
        int shift = config(ConfigShiftWidth).toInt();
        int tab = config(ConfigTabStop).toInt();
        int line = cursorLine() + 1;
        int pos = firstPositionInLine(line);
        QString text = lineContents(line);
        int amount = 0;
        int i = 0;
        for (; i < text.size() && amount < shift; ++i) {
            if (text.at(i) == ' ')
                ++amount;
            else if (text.at(i) == '\t')
                amount += tab; // FIXME: take position into consideration
            else
                break;
        }
        removeText(Range(pos, pos+i));
    } else if (input.isControl('p') || input.isControl('n')) {
        QTextCursor tc = m_cursor;
        moveToNextWordStart(1, false, false);
        QString str = selectText(Range(position(), tc.position()));
        m_cursor = tc;
        q->simpleCompletionRequested(str, input.isControl('n'));
    } else if (input.isShift(Qt::Key_Insert)) {
        // Insert text from clipboard.
        QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *data = clipboard->mimeData();
        if (data && data->hasText())
            insertInInsertMode(data->text());
    } else {
        m_buffer->insertState.insertingSpaces = input.isKey(Key_Space);
        if (!handleInsertInEditor(input)) {
            const QString toInsert = input.text();
            if (toInsert.isEmpty())
                return;
            insertInInsertMode(toInsert);
        }
        m_buffer->insertState.insertingSpaces = false;
    }
}

void FakeVimHandler::Private::insertInInsertMode(const QString &text)
{
    joinPreviousEditBlock();
    insertText(text);
    if (hasConfig(ConfigSmartIndent) && isElectricCharacter(text.at(0))) {
        const QString leftText = block().text()
               .left(position() - 1 - block().position());
        if (leftText.simplified().isEmpty()) {
            Range range(position(), position(), g.rangemode);
            indentText(range, text.at(0));
        }
    }
    setTargetColumn();
    endEditBlock();
    g.submode = NoSubMode;
}

bool FakeVimHandler::Private::startRecording(const Input &input)
{
    QChar reg = input.asChar();
    if (reg == '"' || reg.isLetterOrNumber()) {
        g.currentRegister = reg.unicode();
        g.isRecording = true;
        g.recorded.clear();
        return true;
    }

    return false;
}

void FakeVimHandler::Private::record(const Input &input)
{
    if (g.isRecording)
        g.recorded.append(input.toString());
}

void FakeVimHandler::Private::stopRecording()
{
    // Remove q from end (stop recording command).
    g.isRecording = false;
    g.recorded.chop(1);
    setRegister(g.currentRegister, g.recorded, g.rangemode);
    g.currentRegister = 0;
    g.recorded.clear();
}

void FakeVimHandler::Private::handleAs(const QString &command)
{
    QString cmd = QString("\"%1").arg(QChar(m_register));

    if (command.contains("%1"))
        cmd.append(command.arg(count()));
    else
        cmd.append(command);

    leaveVisualMode();
    beginLargeEditBlock();
    replay(cmd);
    endEditBlock();
}

bool FakeVimHandler::Private::executeRegister(int reg)
{
    QChar regChar(reg);

    // TODO: Prompt for an expression to execute if register is '='.
    if (reg == '@' && g.lastExecutedRegister != 0)
        reg = g.lastExecutedRegister;
    else if (QString("\".*+").contains(regChar) || regChar.isLetterOrNumber())
        g.lastExecutedRegister = reg;
    else
        return false;

    // FIXME: In Vim it's possible to interrupt recursive macro with <C-c>.
    //        One solution may be to call QApplication::processEvents() and check if <C-c> was
    //        used when a mapping is active.
    // According to Vim, register is executed like mapping.
    prependMapping(Inputs(registerContents(reg), false, false));

    return true;
}

EventResult FakeVimHandler::Private::handleExMode(const Input &input)
{
    // handle C-R, C-R C-W, C-R {register}
    if (handleCommandBufferPaste(input))
        return EventHandled;

    if (input.isEscape()) {
        g.commandBuffer.clear();
        leaveCurrentMode();
        g.submode = NoSubMode;
    } else if (g.submode == CtrlVSubMode) {
        g.commandBuffer.insertChar(input.raw());
        g.submode = NoSubMode;
    } else if (input.isControl('v')) {
        g.submode = CtrlVSubMode;
        g.subsubmode = NoSubSubMode;
        return EventHandled;
    } else if (input.isBackspace()) {
        if (g.commandBuffer.isEmpty()) {
            leaveVisualMode();
            leaveCurrentMode();
        } else if (g.commandBuffer.hasSelection()) {
            g.commandBuffer.deleteSelected();
        } else {
            g.commandBuffer.deleteChar();
        }
    } else if (input.isKey(Key_Tab)) {
        // FIXME: Complete actual commands.
        g.commandBuffer.historyUp();
    } else if (input.isReturn()) {
        showMessage(MessageCommand, g.commandBuffer.display());
        handleExCommand(g.commandBuffer.contents());
        g.commandBuffer.clear();
    } else if (!g.commandBuffer.handleInput(input)) {
        qDebug() << "IGNORED IN EX-MODE: " << input.key() << input.text();
        return EventUnhandled;
    }

    return EventHandled;
}

EventResult FakeVimHandler::Private::handleSearchSubSubMode(const Input &input)
{
    EventResult handled = EventHandled;

    // handle C-R, C-R C-W, C-R {register}
    if (handleCommandBufferPaste(input))
        return handled;

    if (input.isEscape()) {
        g.currentMessage.clear();
        setPosition(m_searchStartPosition);
        scrollToLine(m_searchFromScreenLine);
    } else if (input.isBackspace()) {
        if (g.searchBuffer.isEmpty())
            leaveCurrentMode();
        else
            g.searchBuffer.deleteChar();
    } else if (input.isReturn()) {
        const QString &needle = g.searchBuffer.contents();
        if (!needle.isEmpty())
            g.lastSearch = needle;
        else
            g.searchBuffer.setContents(g.lastSearch);

        updateFind(true);

        if (finishSearch()) {
            if (g.submode != NoSubMode)
                finishMovement(g.searchBuffer.prompt() + g.lastSearch + '\n');
            if (g.currentMessage.isEmpty())
                showMessage(MessageCommand, g.searchBuffer.display());
        } else {
            handled = EventCancelled; // Not found so cancel mapping if any.
        }
    } else if (input.isKey(Key_Tab)) {
        g.searchBuffer.insertChar(QChar(9));
    } else if (!g.searchBuffer.handleInput(input)) {
        //qDebug() << "IGNORED IN SEARCH MODE: " << input.key() << input.text();
        return EventUnhandled;
    }

    if (input.isReturn() || input.isEscape()) {
        g.searchBuffer.clear();
        leaveCurrentMode();
    } else {
        updateFind(false);
    }

    return handled;
}

// This uses 0 based line counting (hidden lines included).
int FakeVimHandler::Private::parseLineAddress(QString *cmd)
{
    //qDebug() << "CMD: " << cmd;
    if (cmd->isEmpty())
        return -1;

    int result = -1;
    QChar c = cmd->at(0);
    if (c == '.') { // current line
        result = cursorBlockNumber();
        cmd->remove(0, 1);
    } else if (c == '$') { // last line
        result = document()->blockCount() - 1;
        cmd->remove(0, 1);
    } else if (c == '\'') { // mark
        cmd->remove(0, 1);
        if (cmd->isEmpty()) {
            showMessage(MessageError, msgMarkNotSet(QString()));
            return -1;
        }
        c = cmd->at(0);
        Mark m = mark(c);
        if (!m.isValid() || !m.isLocal(m_currentFileName)) {
            showMessage(MessageError, msgMarkNotSet(c));
            return -1;
        }
        cmd->remove(0, 1);
        result = m.position(document()).line;
    } else if (c.isDigit()) { // line with given number
        result = 0;
    } else if (c == '-' || c == '+') { // add or subtract from current line number
        result = cursorBlockNumber();
    } else if (c == '/' || c == '?'
        || (c == '\\' && cmd->size() > 1 && QString("/?&").contains(cmd->at(1)))) {
        // search for expression
        SearchData sd;
        if (c == '/' || c == '?') {
            const int end = findUnescaped(c, *cmd, 1);
            if (end == -1)
                return -1;
            sd.needle = cmd->mid(1, end - 1);
            cmd->remove(0, end + 1);
        } else {
            c = cmd->at(1);
            cmd->remove(0, 2);
            sd.needle = (c == '&') ? g.lastSubstitutePattern : g.lastSearch;
        }
        sd.forward = (c != '?');
        const QTextBlock b = block();
        const int pos = b.position() + (sd.forward ? b.length() - 1 : 0);
        QTextCursor tc = search(sd, pos, 1, true);
        g.lastSearch = sd.needle;
        if (tc.isNull())
            return -1;
        result = tc.block().blockNumber();
    } else {
        return cursorBlockNumber();
    }

    // basic arithmetic ("-3+5" or "++" means "+2" etc.)
    int n = 0;
    bool add = true;
    int i = 0;
    for (; i < cmd->size(); ++i) {
        c = cmd->at(i);
        if (c == '-' || c == '+') {
            if (n != 0)
                result = result + (add ? n - 1 : -(n - 1));
            add = c == '+';
            result = result + (add ? 1 : -1);
            n = 0;
        } else if (c.isDigit()) {
            n = n * 10 + c.digitValue();
        } else if (!c.isSpace()) {
            break;
        }
    }
    if (n != 0)
        result = result + (add ? n - 1 : -(n - 1));
    *cmd = cmd->mid(i).trimmed();

    return result;
}

void FakeVimHandler::Private::setCurrentRange(const Range &range)
{
    setAnchorAndPosition(range.beginPos, range.endPos);
    g.rangemode = range.rangemode;
}

bool FakeVimHandler::Private::parseExCommand(QString *line, ExCommand *cmd)
{
    *cmd = ExCommand();
    if (line->isEmpty())
        return false;

    // parse range first
    if (!parseLineRange(line, cmd))
        return false;

    // get first command from command line
    QChar close;
    bool subst = false;
    int i = 0;
    for (; i < line->size(); ++i) {
        const QChar &c = line->at(i);
        if (c == '\\') {
            ++i; // skip escaped character
        } else if (close.isNull()) {
            if (c == '|') {
                // split on |
                break;
            } else if (c == '/') {
                subst = i > 0 && (line->at(i - 1) == 's');
                close = c;
            } else if (c == '"' || c == '\'') {
                close = c;
            }
        } else if (c == close) {
            if (subst)
                subst = false;
            else
                close = QChar();
        }
    }

    cmd->cmd = line->mid(0, i).trimmed();

    // command arguments starts with first non-letter character
    cmd->args = cmd->cmd.section(QRegularExpression("(?=[^a-zA-Z])"), 1);
    if (!cmd->args.isEmpty()) {
        cmd->cmd.chop(cmd->args.size());
        cmd->args = cmd->args.trimmed();

        // '!' at the end of command
        cmd->hasBang = cmd->args.startsWith('!');
        if (cmd->hasBang)
            cmd->args = cmd->args.mid(1).trimmed();
    }

    // remove the first command from command line
    line->remove(0, i + 1);

    return true;
}

bool FakeVimHandler::Private::parseLineRange(QString *line, ExCommand *cmd)
{
    // remove leading colons and spaces
    line->remove(QRegularExpression("^\\s*(:+\\s*)*"));

    // special case ':!...' (use invalid range)
    if (line->startsWith('!')) {
        cmd->range = Range();
        return true;
    }

    // FIXME: that seems to be different for %w and %s
    if (line->startsWith('%'))
        line->replace(0, 1, "1,$");

    int beginLine = parseLineAddress(line);
    int endLine;
    if (line->startsWith(',')) {
        *line = line->mid(1).trimmed();
        endLine = parseLineAddress(line);
    } else {
        endLine = beginLine;
    }
    if (beginLine == -1 || endLine == -1)
        return false;

    const int beginPos = firstPositionInLine(qMin(beginLine, endLine) + 1, false);
    const int endPos = lastPositionInLine(qMax(beginLine, endLine) + 1, false);
    cmd->range = Range(beginPos, endPos, RangeLineMode);
    cmd->count = beginLine;

    return true;
}

void FakeVimHandler::Private::parseRangeCount(const QString &line, Range *range) const
{
    bool ok;
    const int count = qAbs(line.trimmed().toInt(&ok));
    if (ok) {
        const int beginLine = blockAt(range->endPos).blockNumber() + 1;
        const int endLine = qMin(beginLine + count - 1, document()->blockCount());
        range->beginPos = firstPositionInLine(beginLine, false);
        range->endPos = lastPositionInLine(endLine, false);
    }
}

// use handleExCommand for invoking commands that might move the cursor
void FakeVimHandler::Private::handleCommand(const QString &cmd)
{
    handleExCommand(cmd);
}

bool FakeVimHandler::Private::handleExSubstituteCommand(const ExCommand &cmd)
{
    // :substitute
    if (!cmd.matches("s", "substitute")
        && !(cmd.cmd.isEmpty() && !cmd.args.isEmpty() && QString("&~").contains(cmd.args[0]))) {
        return false;
    }

    int count = 1;
    QString line = cmd.args;
    const int countIndex = line.lastIndexOf(QRegularExpression("\\d+$"));
    if (countIndex != -1) {
        count = line.midRef(countIndex).toInt();
        line = line.mid(0, countIndex).trimmed();
    }

    if (cmd.cmd.isEmpty()) {
        // keep previous substitution flags on '&&' and '~&'
        if (line.size() > 1 && line[1] == '&')
            g.lastSubstituteFlags += line.midRef(2);
        else
            g.lastSubstituteFlags = line.mid(1);
        if (line[0] == '~')
            g.lastSubstitutePattern = g.lastSearch;
    } else {
        if (line.isEmpty()) {
            g.lastSubstituteFlags.clear();
        } else {
            // we have /{pattern}/{string}/[flags]  now
            const QChar separator = line.at(0);
            int pos1 = findUnescaped(separator, line, 1);
            if (pos1 == -1)
                return false;
            int pos2 = findUnescaped(separator, line, pos1 + 1);
            if (pos2 == -1)
                pos2 = line.size();

            g.lastSubstitutePattern = line.mid(1, pos1 - 1);
            g.lastSubstituteReplacement = line.mid(pos1 + 1, pos2 - pos1 - 1);
            g.lastSubstituteFlags = line.mid(pos2 + 1);
        }
    }

    count = qMax(1, count);
    QString needle = g.lastSubstitutePattern;

    if (g.lastSubstituteFlags.contains('i'))
        needle.prepend("\\c");

    QRegularExpression pattern = vimPatternToQtPattern(needle, hasConfig(ConfigIgnoreCase),
                                            hasConfig(ConfigSmartCase));

    QTextBlock lastBlock;
    QTextBlock firstBlock;
    const bool global = g.lastSubstituteFlags.contains('g');
    for (int a = 0; a != count; ++a) {
        for (QTextBlock block = blockAt(cmd.range.endPos);
            block.isValid() && block.position() + block.length() > cmd.range.beginPos;
            block = block.previous()) {
            QString text = block.text();
            if (substituteText(&text, pattern, g.lastSubstituteReplacement, global)) {
                firstBlock = block;
                if (!lastBlock.isValid()) {
                    lastBlock = block;
                    beginEditBlock();
                }
                QTextCursor tc = m_cursor;
                const int pos = block.position();
                const int anchor = pos + block.length() - 1;
                tc.setPosition(anchor);
                tc.setPosition(pos, KeepAnchor);
                tc.insertText(text);
            }
        }
    }

    if (lastBlock.isValid()) {
        m_buffer->undoState.position = CursorPosition(firstBlock.blockNumber(), 0);

        leaveVisualMode();
        setPosition(lastBlock.position());
        setAnchor();
        moveToFirstNonBlankOnLine();

        endEditBlock();
    }

    return true;
}

bool FakeVimHandler::Private::handleExTabNextCommand(const ExCommand &cmd)
{
    if (!cmd.matches("tabn", "tabnext"))
        return false;

    q->tabNextRequested();
    return true;
}

bool FakeVimHandler::Private::handleExTabPreviousCommand(const ExCommand &cmd)
{
    if (!cmd.matches("tabp", "tabprevious"))
        return false;

    q->tabPreviousRequested();
    return true;
}

bool FakeVimHandler::Private::handleExMapCommand(const ExCommand &cmd0) // :map
{
    QByteArray modes;
    enum Type { Map, Noremap, Unmap } type;

    QByteArray cmd = cmd0.cmd.toLatin1();

    // Strange formatting. But everything else is even uglier.
    if (cmd == "map") { modes = "nvo"; type = Map; } else
    if (cmd == "nm" || cmd == "nmap") { modes = "n"; type = Map; } else
    if (cmd == "vm" || cmd == "vmap") { modes = "v"; type = Map; } else
    if (cmd == "xm" || cmd == "xmap") { modes = "x"; type = Map; } else
    if (cmd == "smap") { modes = "s"; type = Map; } else
    if (cmd == "omap") { modes = "o"; type = Map; } else
    if (cmd == "map!") { modes = "ic"; type = Map; } else
    if (cmd == "im" || cmd == "imap") { modes = "i"; type = Map; } else
    if (cmd == "lm" || cmd == "lmap") { modes = "l"; type = Map; } else
    if (cmd == "cm" || cmd == "cmap") { modes = "c"; type = Map; } else

    if (cmd == "no" || cmd == "noremap") { modes = "nvo"; type = Noremap; } else
    if (cmd == "nn" || cmd == "nnoremap") { modes = "n"; type = Noremap; } else
    if (cmd == "vn" || cmd == "vnoremap") { modes = "v"; type = Noremap; } else
    if (cmd == "xn" || cmd == "xnoremap") { modes = "x"; type = Noremap; } else
    if (cmd == "snor" || cmd == "snoremap") { modes = "s"; type = Noremap; } else
    if (cmd == "ono" || cmd == "onoremap") { modes = "o"; type = Noremap; } else
    if (cmd == "no!" || cmd == "noremap!") { modes = "ic"; type = Noremap; } else
    if (cmd == "ino" || cmd == "inoremap") { modes = "i"; type = Noremap; } else
    if (cmd == "ln" || cmd == "lnoremap") { modes = "l"; type = Noremap; } else
    if (cmd == "cno" || cmd == "cnoremap") { modes = "c"; type = Noremap; } else

    if (cmd == "unm" || cmd == "unmap") { modes = "nvo"; type = Unmap; } else
    if (cmd == "nun" || cmd == "nunmap") { modes = "n"; type = Unmap; } else
    if (cmd == "vu" || cmd == "vunmap") { modes = "v"; type = Unmap; } else
    if (cmd == "xu" || cmd == "xunmap") { modes = "x"; type = Unmap; } else
    if (cmd == "sunm" || cmd == "sunmap") { modes = "s"; type = Unmap; } else
    if (cmd == "ou" || cmd == "ounmap") { modes = "o"; type = Unmap; } else
    if (cmd == "unm!" || cmd == "unmap!") { modes = "ic"; type = Unmap; } else
    if (cmd == "iu" || cmd == "iunmap") { modes = "i"; type = Unmap; } else
    if (cmd == "lu" || cmd == "lunmap") { modes = "l"; type = Unmap; } else
    if (cmd == "cu" || cmd == "cunmap") { modes = "c"; type = Unmap; }

    else
        return false;

    QString args = cmd0.args;
    bool silent = false;
    bool unique = false;
    forever {
        if (eatString("<silent>", &args)) {
            silent = true;
        } else if (eatString("<unique>", &args)) {
            continue;
        } else if (eatString("<special>", &args)) {
            continue;
        } else if (eatString("<buffer>", &args)) {
            notImplementedYet();
            continue;
        } else if (eatString("<script>", &args)) {
            notImplementedYet();
            continue;
        } else if (eatString("<expr>", &args)) {
            notImplementedYet();
            return true;
        }
        break;
    }

    const QString lhs = args.section(QRegularExpression("\\s+"), 0, 0);
    const QString rhs = args.section(QRegularExpression("\\s+"), 1);
    if ((rhs.isNull() && type != Unmap) || (!rhs.isNull() && type == Unmap)) {
        // FIXME: Dump mappings here.
        //qDebug() << g.mappings;
        return true;
    }

    Inputs key(lhs);
    //qDebug() << "MAPPING: " << modes << lhs << rhs;
    switch (type) {
        case Unmap:
            foreach (char c, modes)
                MappingsIterator(&g.mappings, c, key).remove();
            break;
        case Map:
        case Noremap: {
            Inputs inputs(rhs, type == Noremap, silent);
            foreach (char c, modes)
                MappingsIterator(&g.mappings, c).setInputs(key, inputs, unique);
            break;
        }
    }
    return true;
}

bool FakeVimHandler::Private::handleExHistoryCommand(const ExCommand &cmd)
{
    // :his[tory]
    if (!cmd.matches("his", "history"))
        return false;

    if (cmd.args.isEmpty()) {
        QString info;
        info += "#  command history\n";
        int i = 0;
        foreach (const QString &item, g.commandBuffer.historyItems()) {
            ++i;
            info += QString("%1 %2\n").arg(i, -8).arg(item);
        }
        q->extraInformationChanged(info);
    } else {
        notImplementedYet();
    }

    return true;
}

bool FakeVimHandler::Private::handleExRegisterCommand(const ExCommand &cmd)
{
    // :reg[isters] and :di[splay]
    if (!cmd.matches("reg", "registers") && !cmd.matches("di", "display"))
        return false;

    QByteArray regs = cmd.args.toLatin1();
    if (regs.isEmpty()) {
        regs = "\"0123456789";
        for (auto it = g.registers.cbegin(), end = g.registers.cend(); it != end; ++it) {
            if (it.key() > '9')
                regs += char(it.key());
        }
    }
    QString info;
    info += "--- Registers ---\n";
    foreach (char reg, regs) {
        QString value = quoteUnprintable(registerContents(reg));
        info += QString("\"%1   %2\n").arg(reg).arg(value);
    }
    q->extraInformationChanged(info);

    return true;
}

bool FakeVimHandler::Private::handleExSetCommand(const ExCommand &cmd)
{
    // :se[t]
    if (!cmd.matches("se", "set"))
        return false;

    clearMessage();

    if (cmd.args.contains('=')) {
        // Non-boolean config to set.
        int p = cmd.args.indexOf('=');
        QString error = theFakeVimSettings()
                ->trySetValue(cmd.args.left(p), cmd.args.mid(p + 1));
        if (!error.isEmpty())
            showMessage(MessageError, error);
    } else {
        QString optionName = cmd.args;

        bool toggleOption = optionName.endsWith('!');
        bool printOption = !toggleOption && optionName.endsWith('?');
        if (printOption || toggleOption)
            optionName.chop(1);

        bool negateOption = optionName.startsWith("no");
        if (negateOption)
            optionName.remove(0, 2);

        FakeVimAction *act = theFakeVimSettings()->item(optionName);
        if (!act) {
            showMessage(MessageError, Tr::tr("Unknown option:") + ' ' + cmd.args);
        } else if (act->defaultValue().type() == QVariant::Bool) {
            bool oldValue = act->value().toBool();
            if (printOption) {
                showMessage(MessageInfo, QLatin1String(oldValue ? "" : "no")
                            + act->settingsKey().toLower());
            } else if (toggleOption || negateOption == oldValue) {
                act->setValue(!oldValue);
            }
        } else if (negateOption && !printOption) {
            showMessage(MessageError, Tr::tr("Invalid argument:") + ' ' + cmd.args);
        } else if (toggleOption) {
            showMessage(MessageError, Tr::tr("Trailing characters:") + ' ' + cmd.args);
        } else {
            showMessage(MessageInfo, act->settingsKey().toLower() + "="
                        + act->value().toString());
        }
    }
    updateEditor();
    updateHighlights();
    return true;
}

bool FakeVimHandler::Private::handleExNormalCommand(const ExCommand &cmd)
{
    // :norm[al]
    if (!cmd.matches("norm", "normal"))
        return false;
    //qDebug() << "REPLAY NORMAL: " << quoteUnprintable(reNormal.cap(3));
    replay(cmd.args);
    return true;
}

bool FakeVimHandler::Private::handleExYankDeleteCommand(const ExCommand &cmd)
{
    // :[range]d[elete] [x] [count]
    // :[range]y[ank] [x] [count]
    const bool remove = cmd.matches("d", "delete");
    if (!remove && !cmd.matches("y", "yank"))
        return false;

    // get register from arguments
    const bool hasRegisterArg = !cmd.args.isEmpty() && !cmd.args.at(0).isDigit();
    const int r = hasRegisterArg ? cmd.args.at(0).unicode() : m_register;

    // get [count] from arguments
    Range range = cmd.range;
    parseRangeCount(cmd.args.mid(hasRegisterArg ? 1 : 0).trimmed(), &range);

    yankText(range, r);

    if (remove) {
        leaveVisualMode();
        setPosition(range.beginPos);
        pushUndoState();
        setCurrentRange(range);
        removeText(currentRange());
    }

    return true;
}

bool FakeVimHandler::Private::handleExChangeCommand(const ExCommand &cmd)
{
    // :[range]c[hange]
    if (!cmd.matches("c", "change"))
        return false;

    Range range = cmd.range;
    range.rangemode = RangeLineModeExclusive;
    removeText(range);
    insertAutomaticIndentation(true, cmd.hasBang);

    // FIXME: In Vim same or less number of lines can be inserted and position after insertion is
    //        beginning of last inserted line.
    enterInsertMode();

    return true;
}

bool FakeVimHandler::Private::handleExMoveCommand(const ExCommand &cmd)
{
    // :[range]m[ove] {address}
    if (!cmd.matches("m", "move"))
        return false;

    QString lineCode = cmd.args;

    const int startLine = blockAt(cmd.range.beginPos).blockNumber();
    const int endLine = blockAt(cmd.range.endPos).blockNumber();
    const int lines = endLine - startLine + 1;

    int targetLine = lineCode == "0" ? -1 : parseLineAddress(&lineCode);
    if (targetLine >= startLine && targetLine < endLine) {
        showMessage(MessageError, Tr::tr("Move lines into themselves."));
        return true;
    }

    CursorPosition lastAnchor = markLessPosition();
    CursorPosition lastPosition = markGreaterPosition();

    recordJump();
    setPosition(cmd.range.beginPos);
    pushUndoState();

    setCurrentRange(cmd.range);
    QString text = selectText(cmd.range);
    removeText(currentRange());

    const bool insertAtEnd = targetLine == document()->blockCount();
    if (targetLine >= startLine)
        targetLine -= lines;
    QTextBlock block = document()->findBlockByNumber(insertAtEnd ? targetLine : targetLine + 1);
    setPosition(block.position());
    setAnchor();

    if (insertAtEnd) {
        moveBehindEndOfLine();
        text.chop(1);
        insertText(QString("\n"));
    }
    insertText(text);

    if (!insertAtEnd)
        moveUp(1);
    if (hasConfig(ConfigStartOfLine))
        moveToFirstNonBlankOnLine();

    if (lastAnchor.line >= startLine && lastAnchor.line <= endLine)
        lastAnchor.line += targetLine - startLine + 1;
    if (lastPosition.line >= startLine && lastPosition.line <= endLine)
        lastPosition.line += targetLine - startLine + 1;
    setMark('<', lastAnchor);
    setMark('>', lastPosition);

    if (lines > 2)
        showMessage(MessageInfo, Tr::tr("%n lines moved.", nullptr, lines));

    return true;
}

bool FakeVimHandler::Private::handleExJoinCommand(const ExCommand &cmd)
{
    // :[range]j[oin][!] [count]
    // FIXME: Argument [count] can follow immediately.
    if (!cmd.matches("j", "join"))
        return false;

    // get [count] from arguments
    bool ok;
    int count = cmd.args.toInt(&ok);

    if (ok) {
        setPosition(cmd.range.endPos);
    } else {
        setPosition(cmd.range.beginPos);
        const int startLine = blockAt(cmd.range.beginPos).blockNumber();
        const int endLine = blockAt(cmd.range.endPos).blockNumber();
        count = endLine - startLine + 1;
    }

    moveToStartOfLine();
    pushUndoState();
    joinLines(count, cmd.hasBang);

    moveToFirstNonBlankOnLine();

    return true;
}

bool FakeVimHandler::Private::handleExWriteCommand(const ExCommand &cmd)
{
    // Note: The cmd.args.isEmpty() case is handled by handleExPluginCommand.
    // :w, :x, :wq, ...
    //static QRegularExpression reWrite("^[wx]q?a?!?( (.*))?$");
    if (cmd.cmd != "w" && cmd.cmd != "x" && cmd.cmd != "wq")
        return false;

    int beginLine = lineForPosition(cmd.range.beginPos);
    int endLine = lineForPosition(cmd.range.endPos);
    const bool noArgs = (beginLine == -1);
    if (beginLine == -1)
        beginLine = 0;
    if (endLine == -1)
        endLine = linesInDocument();
    //qDebug() << "LINES: " << beginLine << endLine;
    //QString prefix = cmd.args;
    const bool forced = cmd.hasBang;
    //const bool quit = prefix.contains('q') || prefix.contains('x');
    //const bool quitAll = quit && prefix.contains('a');
    QString fileName = replaceTildeWithHome(cmd.args);
    if (fileName.isEmpty())
        fileName = m_currentFileName;
    QFile file1(fileName);
    const bool exists = file1.exists();
    if (exists && !forced && !noArgs) {
        showMessage(MessageError, Tr::tr
            ("File \"%1\" exists (add ! to override)").arg(fileName));
    } else if (file1.open(QIODevice::ReadWrite)) {
        // Nobody cared, so act ourselves.
        file1.close();
        Range range(firstPositionInLine(beginLine),
            firstPositionInLine(endLine), RangeLineMode);
        QString contents = selectText(range);
        QFile::remove(fileName);
        QFile file2(fileName);
        if (file2.open(QIODevice::ReadWrite)) {
            QTextStream ts(&file2);
            ts << contents;
        } else {
            showMessage(MessageError, Tr::tr
               ("Cannot open file \"%1\" for writing").arg(fileName));
        }
        // Check result by reading back.
        QFile file3(fileName);
        file3.open(QIODevice::ReadOnly);
        QByteArray ba = file3.readAll();
        showMessage(MessageInfo, Tr::tr("\"%1\" %2 %3L, %4C written.")
            .arg(fileName).arg(exists ? QString(" ") : Tr::tr(" [New] "))
            .arg(ba.count('\n')).arg(ba.size()));
        //if (quitAll)
        //    passUnknownExCommand(forced ? "qa!" : "qa");
        //else if (quit)
        //    passUnknownExCommand(forced ? "q!" : "q");
    } else {
        showMessage(MessageError, Tr::tr
            ("Cannot open file \"%1\" for reading").arg(fileName));
    }
    return true;
}

bool FakeVimHandler::Private::handleExReadCommand(const ExCommand &cmd)
{
    // :r[ead]
    if (!cmd.matches("r", "read"))
        return false;

    beginEditBlock();

    moveToStartOfLine();
    moveDown();
    int pos = position();

    m_currentFileName = replaceTildeWithHome(cmd.args);
    QFile file(m_currentFileName);
    file.open(QIODevice::ReadOnly);
    QTextStream ts(&file);
    QString data = ts.readAll();
    insertText(data);

    setAnchorAndPosition(pos, pos);

    endEditBlock();

    showMessage(MessageInfo, Tr::tr("\"%1\" %2L, %3C")
        .arg(m_currentFileName).arg(data.count('\n')).arg(data.size()));

    return true;
}

bool FakeVimHandler::Private::handleExBangCommand(const ExCommand &cmd) // :!
{
    if (!cmd.cmd.isEmpty() || !cmd.hasBang)
        return false;

    bool replaceText = cmd.range.isValid();
    const QString command = QString(cmd.cmd.mid(1) + ' ' + cmd.args).trimmed();
    const QString input = replaceText ? selectText(cmd.range) : QString();

    const QString result = getProcessOutput(command, input);

    if (replaceText) {
        setCurrentRange(cmd.range);
        int targetPosition = firstPositionInLine(lineForPosition(cmd.range.beginPos));
        beginEditBlock();
        removeText(currentRange());
        insertText(result);
        setPosition(targetPosition);
        endEditBlock();
        leaveVisualMode();
        //qDebug() << "FILTER: " << command;
        showMessage(MessageInfo, Tr::tr("%n lines filtered.", nullptr,
            input.count('\n')));
    } else if (!result.isEmpty()) {
        q->extraInformationChanged(result);
    }

    return true;
}

bool FakeVimHandler::Private::handleExShiftCommand(const ExCommand &cmd)
{
    // :[range]{<|>}* [count]
    if (!cmd.cmd.isEmpty() || (!cmd.args.startsWith('<') && !cmd.args.startsWith('>')))
        return false;

    const QChar c = cmd.args.at(0);

    // get number of repetition
    int repeat = 1;
    int i = 1;
    for (; i < cmd.args.size(); ++i) {
        const QChar c2 = cmd.args.at(i);
        if (c2 == c)
            ++repeat;
        else if (!c2.isSpace())
            break;
    }

    // get [count] from arguments
    Range range = cmd.range;
    parseRangeCount(cmd.args.mid(i), &range);

    setCurrentRange(range);
    if (c == '<')
        shiftRegionLeft(repeat);
    else
        shiftRegionRight(repeat);

    leaveVisualMode();

    return true;
}

bool FakeVimHandler::Private::handleExSortCommand(const ExCommand &cmd)
{
    // :[range]sor[t][!] [b][f][i][n][o][r][u][x] [/{pattern}/]
    // FIXME: Only the ! for reverse is implemented.
    if (!cmd.matches("sor", "sort"))
        return false;

    // Force operation on full lines, and full document if only
    // one line (the current one...) is specified
    int beginLine = lineForPosition(cmd.range.beginPos);
    int endLine = lineForPosition(cmd.range.endPos);
    if (beginLine == endLine) {
        beginLine = 0;
        endLine = lineForPosition(lastPositionInDocument());
    }
    Range range(firstPositionInLine(beginLine),
                firstPositionInLine(endLine), RangeLineMode);

    QString input = selectText(range);
    if (input.endsWith('\n')) // It should always...
        input.chop(1);

    QStringList lines = input.split('\n');
    lines.sort();
    if (cmd.hasBang)
        std::reverse(lines.begin(), lines.end());
    QString res = lines.join('\n') + '\n';

    replaceText(range, res);

    return true;
}

bool FakeVimHandler::Private::handleExNohlsearchCommand(const ExCommand &cmd)
{
    // :noh, :nohl, ..., :nohlsearch
    if (cmd.cmd.size() < 3 || !QString("nohlsearch").startsWith(cmd.cmd))
        return false;

    g.highlightsCleared = true;
    updateHighlights();
    return true;
}

bool FakeVimHandler::Private::handleExUndoRedoCommand(const ExCommand &cmd)
{
    // :undo
    // :redo
    bool undo = (cmd.cmd == "u" || cmd.cmd == "un" || cmd.cmd == "undo");
    if (!undo && cmd.cmd != "red" && cmd.cmd != "redo")
        return false;

    undoRedo(undo);

    return true;
}

bool FakeVimHandler::Private::handleExGotoCommand(const ExCommand &cmd)
{
    // :{address}
    if (!cmd.cmd.isEmpty() || !cmd.args.isEmpty())
        return false;

    const int beginLine = lineForPosition(cmd.range.endPos);
    setPosition(firstPositionInLine(beginLine));
    clearMessage();
    return true;
}

bool FakeVimHandler::Private::handleExSourceCommand(const ExCommand &cmd)
{
    // :source
    if (cmd.cmd != "so" && cmd.cmd != "source")
        return false;

    QString fileName = replaceTildeWithHome(cmd.args);
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        showMessage(MessageError, Tr::tr("Cannot open file %1").arg(fileName));
        return true;
    }

    bool inFunction = false;
    QByteArray line;
    while (!file.atEnd() || !line.isEmpty()) {
        QByteArray nextline = !file.atEnd() ? file.readLine() : QByteArray();

        //  remove comment
        int i = nextline.lastIndexOf('"');
        if (i != -1)
            nextline = nextline.remove(i, nextline.size() - i);

        nextline = nextline.trimmed();

        // multi-line command?
        if (nextline.startsWith('\\')) {
            line += nextline.mid(1);
            continue;
        }

        if (line.startsWith("function")) {
            //qDebug() << "IGNORING FUNCTION" << line;
            inFunction = true;
        } else if (inFunction && line.startsWith("endfunction")) {
            inFunction = false;
        } else if (!line.isEmpty() && !inFunction) {
            //qDebug() << "EXECUTING: " << line;
            ExCommand cmd;
            QString commandLine = QString::fromLocal8Bit(line);
            while (parseExCommand(&commandLine, &cmd)) {
                if (!handleExCommandHelper(cmd))
                    break;
            }
        }

        line = nextline;
    }
    file.close();
    return true;
}

bool FakeVimHandler::Private::handleExEchoCommand(const ExCommand &cmd)
{
    // :echo
    if (cmd.cmd != "echo")
        return false;
    showMessage(MessageInfo, cmd.args);
    return true;
}

void FakeVimHandler::Private::handleExCommand(const QString &line0)
{
    QString line = line0; // Make sure we have a copy to prevent aliasing.

    if (line.endsWith('%')) {
        line.chop(1);
        int percent = line.toInt();
        setPosition(firstPositionInLine(percent * linesInDocument() / 100));
        clearMessage();
        return;
    }

    //qDebug() << "CMD: " << cmd;

    enterCommandMode(g.returnToMode);

    beginLargeEditBlock();
    ExCommand cmd;
    QString lastCommand = line;
    while (parseExCommand(&line, &cmd)) {
        if (!handleExCommandHelper(cmd)) {
            showMessage(MessageError, Tr::tr("Not an editor command: %1").arg(lastCommand));
            break;
        }
        lastCommand = line;
    }

    // if the last command closed the editor, we would crash here (:vs and then :on)
    if (!(m_textedit || m_plaintextedit))
        return;

    endEditBlock();

    if (isVisualMode())
        leaveVisualMode();
    leaveCurrentMode();
}

bool FakeVimHandler::Private::handleExCommandHelper(ExCommand &cmd)
{
    return handleExPluginCommand(cmd)
        || handleExGotoCommand(cmd)
        || handleExBangCommand(cmd)
        || handleExHistoryCommand(cmd)
        || handleExRegisterCommand(cmd)
        || handleExYankDeleteCommand(cmd)
        || handleExChangeCommand(cmd)
        || handleExMoveCommand(cmd)
        || handleExJoinCommand(cmd)
        || handleExMapCommand(cmd)
        || handleExNohlsearchCommand(cmd)
        || handleExNormalCommand(cmd)
        || handleExReadCommand(cmd)
        || handleExUndoRedoCommand(cmd)
        || handleExSetCommand(cmd)
        || handleExShiftCommand(cmd)
        || handleExSortCommand(cmd)
        || handleExSourceCommand(cmd)
        || handleExSubstituteCommand(cmd)
        || handleExTabNextCommand(cmd)
        || handleExTabPreviousCommand(cmd)
        || handleExWriteCommand(cmd)
        || handleExEchoCommand(cmd);
}

bool FakeVimHandler::Private::handleExPluginCommand(const ExCommand &cmd)
{
    bool handled = false;
    int pos = m_cursor.position();
    commitCursor();
    q->handleExCommandRequested(&handled, cmd);
    //qDebug() << "HANDLER REQUEST: " << cmd.cmd << handled;
    if (handled && (m_textedit || m_plaintextedit)) {
        pullCursor();
        if (m_cursor.position() != pos)
            recordJump(pos);
    }
    return handled;
}

void FakeVimHandler::Private::searchBalanced(bool forward, QChar needle, QChar other)
{
    int level = 1;
    int pos = position();
    const int npos = forward ? lastPositionInDocument() : 0;
    while (true) {
        if (forward)
            ++pos;
        else
            --pos;
        if (pos == npos)
            return;
        QChar c = characterAt(pos);
        if (c == other)
            ++level;
        else if (c == needle)
            --level;
        if (level == 0) {
            const int oldLine = cursorLine() - cursorLineOnScreen();
            // Making this unconditional feels better, but is not "vim like".
            if (oldLine != cursorLine() - cursorLineOnScreen())
                scrollToLine(cursorLine() - linesOnScreen() / 2);
            recordJump();
            setPosition(pos);
            setTargetColumn();
            return;
        }
    }
}

QTextCursor FakeVimHandler::Private::search(const SearchData &sd, int startPos, int count,
    bool showMessages)
{
    QRegularExpression needleExp = vimPatternToQtPattern(sd.needle, hasConfig(ConfigIgnoreCase),
                                              hasConfig(ConfigSmartCase));
    if (!needleExp.isValid()) {
        if (showMessages) {
            QString error = needleExp.errorString();
            showMessage(MessageError, Tr::tr("Invalid regular expression: %1").arg(error));
        }
        if (sd.highlightMatches)
            highlightMatches(QString());
        return QTextCursor();
    }

    int repeat = count;
    const int pos = startPos + (sd.forward ? 1 : -1);

    QTextCursor tc;
    if (pos >= 0 && pos < document()->characterCount()) {
        tc = QTextCursor(document());
        tc.setPosition(pos);
        if (sd.forward && afterEndOfLine(document(), pos))
            tc.movePosition(Right);

        if (!tc.isNull()) {
            if (sd.forward)
                searchForward(&tc, needleExp, &repeat);
            else
                searchBackward(&tc, needleExp, &repeat);
        }
    }

    if (tc.isNull()) {
        if (hasConfig(ConfigWrapScan)) {
            tc = QTextCursor(document());
            tc.movePosition(sd.forward ? StartOfDocument : EndOfDocument);
            if (sd.forward)
                searchForward(&tc, needleExp, &repeat);
            else
                searchBackward(&tc, needleExp, &repeat);
            if (tc.isNull()) {
                if (showMessages) {
                    showMessage(MessageError,
                        Tr::tr("Pattern not found: %1").arg(sd.needle));
                }
            } else if (showMessages) {
                QString msg = sd.forward
                    ? Tr::tr("Search hit BOTTOM, continuing at TOP.")
                    : Tr::tr("Search hit TOP, continuing at BOTTOM.");
                showMessage(MessageWarning, msg);
            }
        } else if (showMessages) {
            QString msg = sd.forward
                ? Tr::tr("Search hit BOTTOM without match for: %1")
                : Tr::tr("Search hit TOP without match for: %1");
            showMessage(MessageError, msg.arg(sd.needle));
        }
    }

    if (sd.highlightMatches)
        highlightMatches(needleExp.pattern());

    return tc;
}

void FakeVimHandler::Private::search(const SearchData &sd, bool showMessages)
{
    const int oldLine = cursorLine() - cursorLineOnScreen();

    QTextCursor tc = search(sd, m_searchStartPosition, count(), showMessages);
    if (tc.isNull()) {
        tc = m_cursor;
        tc.setPosition(m_searchStartPosition);
    }

    if (isVisualMode()) {
        int d = tc.anchor() - tc.position();
        setPosition(tc.position() + d);
    } else {
        // Set Cursor. In contrast to the main editor we have the cursor
        // position before the anchor position.
        setAnchorAndPosition(tc.position(), tc.anchor());
    }

    // Making this unconditional feels better, but is not "vim like".
    if (oldLine != cursorLine() - cursorLineOnScreen())
        scrollToLine(cursorLine() - linesOnScreen() / 2);

    m_searchCursor = m_cursor;

    setTargetColumn();
}

bool FakeVimHandler::Private::searchNext(bool forward)
{
    SearchData sd;
    sd.needle = g.lastSearch;
    sd.forward = forward ? g.lastSearchForward : !g.lastSearchForward;
    sd.highlightMatches = true;
    m_searchStartPosition = position();
    showMessage(MessageCommand, QLatin1Char(g.lastSearchForward ? '/' : '?') + sd.needle);
    recordJump();
    search(sd);
    return finishSearch();
}

void FakeVimHandler::Private::highlightMatches(const QString &needle)
{
    g.lastNeedle = needle;
    g.highlightsCleared = false;
    updateHighlights();
}

void FakeVimHandler::Private::moveToFirstNonBlankOnLine()
{
    g.movetype = MoveLineWise;
    moveToFirstNonBlankOnLine(&m_cursor);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToFirstNonBlankOnLine(QTextCursor *tc)
{
    tc->setPosition(tc->block().position(), KeepAnchor);
    moveToNonBlankOnLine(tc);
}

void FakeVimHandler::Private::moveToFirstNonBlankOnLineVisually()
{
    moveToStartOfLineVisually();
    moveToNonBlankOnLine(&m_cursor);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToNonBlankOnLine(QTextCursor *tc)
{
    const QTextBlock block = tc->block();
    const int maxPos = block.position() + block.length() - 1;
    int i = tc->position();
    while (characterAt(i).isSpace() && i < maxPos)
        ++i;
    tc->setPosition(i, KeepAnchor);
}

void FakeVimHandler::Private::indentSelectedText(QChar typedChar)
{
    beginEditBlock();
    setTargetColumn();
    int beginLine = qMin(lineForPosition(position()), lineForPosition(anchor()));
    int endLine = qMax(lineForPosition(position()), lineForPosition(anchor()));

    Range range(anchor(), position(), g.rangemode);
    indentText(range, typedChar);

    setPosition(firstPositionInLine(beginLine));
    handleStartOfLine();
    setTargetColumn();
    setDotCommand("%1==", endLine - beginLine + 1);
    endEditBlock();

    const int lines = endLine - beginLine + 1;
    if (lines > 2)
        showMessage(MessageInfo, Tr::tr("%n lines indented.", nullptr, lines));
}

void FakeVimHandler::Private::indentText(const Range &range, QChar typedChar)
{
    int beginBlock = blockAt(range.beginPos).blockNumber();
    int endBlock = blockAt(range.endPos).blockNumber();
    if (beginBlock > endBlock)
        std::swap(beginBlock, endBlock);

    // Don't remember current indentation in last text insertion.
    const QString lastInsertion = m_buffer->lastInsertion;
    q->indentRegion(beginBlock, endBlock, typedChar);
    m_buffer->lastInsertion = lastInsertion;
}

bool FakeVimHandler::Private::isElectricCharacter(QChar c) const
{
    bool result = false;
    q->checkForElectricCharacter(&result, c);
    return result;
}

void FakeVimHandler::Private::shiftRegionRight(int repeat)
{
    int beginLine = lineForPosition(anchor());
    int endLine = lineForPosition(position());
    int targetPos = anchor();
    if (beginLine > endLine) {
        std::swap(beginLine, endLine);
        targetPos = position();
    }
    if (hasConfig(ConfigStartOfLine))
        targetPos = firstPositionInLine(beginLine);

    const int sw = config(ConfigShiftWidth).toInt();
    g.movetype = MoveLineWise;
    beginEditBlock();
    QTextBlock block = document()->findBlockByLineNumber(beginLine - 1);
    while (block.isValid() && lineNumber(block) <= endLine) {
        const Column col = indentation(block.text());
        QTextCursor tc = m_cursor;
        tc.setPosition(block.position());
        if (col.physical > 0)
            tc.setPosition(tc.position() + col.physical, KeepAnchor);
        tc.insertText(tabExpand(col.logical + sw * repeat));
        block = block.next();
    }
    endEditBlock();

    setPosition(targetPos);
    handleStartOfLine();

    const int lines = endLine - beginLine + 1;
    if (lines > 2) {
        showMessage(MessageInfo,
            Tr::tr("%n lines %1ed %2 time.", nullptr, lines)
            .arg(repeat > 0 ? '>' : '<').arg(qAbs(repeat)));
    }
}

void FakeVimHandler::Private::shiftRegionLeft(int repeat)
{
    shiftRegionRight(-repeat);
}

void FakeVimHandler::Private::moveToTargetColumn()
{
    const QTextBlock &bl = block();
    //Column column = cursorColumn();
    //int logical = logical
    const int pos = lastPositionInLine(bl.blockNumber() + 1, false);
    if (m_targetColumn == -1) {
        setPosition(pos);
        return;
    }
    const int physical = bl.position() + logicalToPhysicalColumn(m_targetColumn, bl.text());
    //qDebug() << "CORRECTING COLUMN FROM: " << logical << "TO" << m_targetColumn;
    setPosition(qMin(pos, physical));
}

void FakeVimHandler::Private::setTargetColumn()
{
    m_targetColumn = logicalCursorColumn();
    m_visualTargetColumn = m_targetColumn;

    QTextCursor tc = m_cursor;
    tc.movePosition(StartOfLine);
    m_targetColumnWrapped = m_cursor.position() - tc.position();
}

/* if simple is given:
 *  class 0: spaces
 *  class 1: non-spaces
 * else
 *  class 0: spaces
 *  class 1: non-space-or-letter-or-number
 *  class 2: letter-or-number
 */


int FakeVimHandler::Private::charClass(QChar c, bool simple) const
{
    if (simple)
        return c.isSpace() ? 0 : 1;
    // FIXME: This means that only characters < 256 in the
    // ConfigIsKeyword setting are handled properly.
    if (c.unicode() < 256) {
        //int old = (c.isLetterOrNumber() || c.unicode() == '_') ? 2
        //    :  c.isSpace() ? 0 : 1;
        //qDebug() << c.unicode() << old << m_charClass[c.unicode()];
        return m_charClass[c.unicode()];
    }
    if (c.isLetterOrNumber() || c == '_')
        return 2;
    return c.isSpace() ? 0 : 1;
}

void FakeVimHandler::Private::miniBufferTextEdited(const QString &text, int cursorPos,
    int anchorPos)
{
    if (!isCommandLineMode()) {
        editor()->setFocus();
    } else if (text.isEmpty()) {
        // editing cancelled
        enterFakeVim();
        handleDefaultKey(Input(Qt::Key_Escape, Qt::NoModifier, QString()));
        leaveFakeVim();
        editor()->setFocus();
    } else {
        CommandBuffer &cmdBuf = (g.mode == ExMode) ? g.commandBuffer : g.searchBuffer;
        int pos = qMax(1, cursorPos);
        int anchor = anchorPos == -1 ? pos : qMax(1, anchorPos);
        QString buffer = text;
        // prepend prompt character if missing
        if (!buffer.startsWith(cmdBuf.prompt())) {
            buffer.prepend(cmdBuf.prompt());
            ++pos;
            ++anchor;
        }
        // update command/search buffer
        cmdBuf.setContents(buffer.mid(1), pos - 1, anchor - 1);
        if (pos != cursorPos || anchor != anchorPos || buffer != text)
            q->commandBufferChanged(buffer, pos, anchor, 0);
        // update search expression
        if (g.subsubmode == SearchSubSubMode) {
            updateFind(false);
            commitCursor();
        }
    }
}

void FakeVimHandler::Private::pullOrCreateBufferData()
{
    const QVariant data = document()->property("FakeVimSharedData");
    if (data.isValid()) {
        // FakeVimHandler has been already created for this document (e.g. in other split).
        m_buffer = data.value<BufferDataPtr>();
    } else {
        // FakeVimHandler has not been created for this document yet.
        m_buffer = BufferDataPtr(new BufferData);
        document()->setProperty("FakeVimSharedData", QVariant::fromValue(m_buffer));
    }

    if (editor()->hasFocus())
        m_buffer->currentHandler = this;
}

// Helper to parse a-z,A-Z,48-57,_
static int someInt(const QString &str)
{
    if (str.toInt())
        return str.toInt();
    if (!str.isEmpty())
        return str.at(0).unicode();
    return 0;
}

void FakeVimHandler::Private::setupCharClass()
{
    for (int i = 0; i < 256; ++i) {
        const QChar c = QLatin1Char(i);
        m_charClass[i] = c.isSpace() ? 0 : 1;
    }
    const QString conf = config(ConfigIsKeyword).toString();
    foreach (const QString &part, conf.split(',')) {
        if (part.contains('-')) {
            const int from = someInt(part.section('-', 0, 0));
            const int to = someInt(part.section('-', 1, 1));
            for (int i = qMax(0, from); i <= qMin(255, to); ++i)
                m_charClass[i] = 2;
        } else {
            m_charClass[qMin(255, someInt(part))] = 2;
        }
    }
}

void FakeVimHandler::Private::moveToBoundary(bool simple, bool forward)
{
    QTextCursor tc(document());
    tc.setPosition(position());
    if (forward ? tc.atBlockEnd() : tc.atBlockStart())
        return;

    QChar c = characterAt(tc.position() + (forward ? -1 : 1));
    int lastClass = tc.atStart() ? -1 : charClass(c, simple);
    QTextCursor::MoveOperation op = forward ? Right : Left;
    while (true) {
        c = characterAt(tc.position());
        int thisClass = charClass(c, simple);
        if (thisClass != lastClass || (forward ? tc.atBlockEnd() : tc.atBlockStart())) {
            if (tc != m_cursor)
                tc.movePosition(forward ? Left : Right);
            break;
        }
        lastClass = thisClass;
        tc.movePosition(op);
    }
    setPosition(tc.position());
}

void FakeVimHandler::Private::moveToNextBoundary(bool end, int count, bool simple, bool forward)
{
    int repeat = count;
    while (repeat > 0 && !(forward ? atDocumentEnd() : atDocumentStart())) {
        setPosition(position() + (forward ? 1 : -1));
        moveToBoundary(simple, forward);
        if (atBoundary(end, simple))
            --repeat;
    }
}

void FakeVimHandler::Private::moveToNextBoundaryStart(int count, bool simple, bool forward)
{
    moveToNextBoundary(false, count, simple, forward);
}

void FakeVimHandler::Private::moveToNextBoundaryEnd(int count, bool simple, bool forward)
{
    moveToNextBoundary(true, count, simple, forward);
}

void FakeVimHandler::Private::moveToBoundaryStart(int count, bool simple, bool forward)
{
    moveToNextBoundaryStart(atBoundary(false, simple) ? count - 1 : count, simple, forward);
}

void FakeVimHandler::Private::moveToBoundaryEnd(int count, bool simple, bool forward)
{
    moveToNextBoundaryEnd(atBoundary(true, simple) ? count - 1 : count, simple, forward);
}

void FakeVimHandler::Private::moveToNextWord(bool end, int count, bool simple, bool forward, bool emptyLines)
{
    int repeat = count;
    while (repeat > 0 && !(forward ? atDocumentEnd() : atDocumentStart())) {
        setPosition(position() + (forward ? 1 : -1));
        moveToBoundary(simple, forward);
        if (atWordBoundary(end, simple) && (emptyLines || !atEmptyLine()) )
            --repeat;
    }
}

void FakeVimHandler::Private::moveToNextWordStart(int count, bool simple, bool forward, bool emptyLines)
{
    g.movetype = MoveExclusive;
    moveToNextWord(false, count, simple, forward, emptyLines);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToNextWordEnd(int count, bool simple, bool forward, bool emptyLines)
{
    g.movetype = MoveInclusive;
    moveToNextWord(true, count, simple, forward, emptyLines);
    setTargetColumn();
}

void FakeVimHandler::Private::moveToWordStart(int count, bool simple, bool forward, bool emptyLines)
{
    moveToNextWordStart(atWordStart(simple) ? count - 1 : count, simple, forward, emptyLines);
}

void FakeVimHandler::Private::moveToWordEnd(int count, bool simple, bool forward, bool emptyLines)
{
    moveToNextWordEnd(atWordEnd(simple) ? count - 1 : count, simple, forward, emptyLines);
}

bool FakeVimHandler::Private::handleFfTt(const QString &key, bool repeats)
{
    int key0 = key.size() == 1 ? key.at(0).unicode() : 0;
    // g.subsubmode \in { 'f', 'F', 't', 'T' }
    bool forward = g.subsubdata.is('f') || g.subsubdata.is('t');
    bool exclusive =  g.subsubdata.is('t') || g.subsubdata.is('T');
    int repeat = count();
    int n = block().position() + (forward ? block().length() : - 1);
    const int d = forward ? 1 : -1;
    // FIXME: This also depends on whether 'cpositions' Vim option contains ';'.
    const int skip = (repeats && repeat == 1 && exclusive) ? d : 0;
    int pos = position() + d + skip;

    for (; repeat > 0 && (forward ? pos < n : pos > n); pos += d) {
        if (characterAt(pos).unicode() == key0)
            --repeat;
    }

    if (repeat == 0) {
        setPosition(pos - d - (exclusive ? d : 0));
        setTargetColumn();
        return true;
    }

    return false;
}

void FakeVimHandler::Private::moveToMatchingParanthesis()
{
    bool moved = false;
    bool forward = false;

    const int anc = anchor();
    QTextCursor tc = m_cursor;

    // If no known parenthesis symbol is under cursor find one on the current line after cursor.
    static const QString parenthesesChars("([{}])");
    while (!parenthesesChars.contains(characterAt(tc.position())) && !tc.atBlockEnd())
        tc.setPosition(tc.position() + 1);

    if (tc.atBlockEnd())
        tc = m_cursor;

    q->moveToMatchingParenthesis(&moved, &forward, &tc);
    if (moved) {
        if (forward)
            tc.movePosition(Left, KeepAnchor, 1);
        setAnchorAndPosition(anc, tc.position());
        setTargetColumn();
    }
}

int FakeVimHandler::Private::cursorLineOnScreen() const
{
    if (!editor())
        return 0;
    const QRect rect = EDITOR(cursorRect(m_cursor));
    return rect.height() > 0 ? rect.y() / rect.height() : 0;
}

int FakeVimHandler::Private::linesOnScreen() const
{
    if (!editor())
        return 1;
    const int h = EDITOR(cursorRect(m_cursor)).height();
    return h > 0 ? EDITOR(viewport()->height()) / h : 1;
}

int FakeVimHandler::Private::cursorLine() const
{
    return lineForPosition(position()) - 1;
}

int FakeVimHandler::Private::cursorBlockNumber() const
{
    return blockAt(qMin(anchor(), position())).blockNumber();
}

int FakeVimHandler::Private::physicalCursorColumn() const
{
    return position() - block().position();
}

int FakeVimHandler::Private::physicalToLogicalColumn
    (const int physical, const QString &line) const
{
    const int ts = config(ConfigTabStop).toInt();
    int p = 0;
    int logical = 0;
    while (p < physical) {
        QChar c = line.at(p);
        //if (c == ' ')
        //    ++logical;
        //else
        if (c == '\t')
            logical += ts - logical % ts;
        else
            ++logical;
            //break;
        ++p;
    }
    return logical;
}

int FakeVimHandler::Private::logicalToPhysicalColumn
    (const int logical, const QString &line) const
{
    const int ts = config(ConfigTabStop).toInt();
    int physical = 0;
    for (int l = 0; l < logical && physical < line.size(); ++physical) {
        QChar c = line.at(physical);
        if (c == '\t')
            l += ts - l % ts;
        else
            ++l;
    }
    return physical;
}

int FakeVimHandler::Private::windowScrollOffset() const
{
    return qMin(theFakeVimSetting(ConfigScrollOff)->value().toInt(), linesOnScreen() / 2);
}

int FakeVimHandler::Private::logicalCursorColumn() const
{
    const int physical = physicalCursorColumn();
    const QString line = block().text();
    return physicalToLogicalColumn(physical, line);
}

Column FakeVimHandler::Private::cursorColumn() const
{
    return Column(physicalCursorColumn(), logicalCursorColumn());
}

int FakeVimHandler::Private::linesInDocument() const
{
    if (m_cursor.isNull())
        return 0;
    return document()->blockCount();
}

void FakeVimHandler::Private::scrollToLine(int line)
{
    // Don't scroll if the line is already at the top.
    updateFirstVisibleLine();
    if (line == m_firstVisibleLine)
        return;

    const QTextCursor tc = m_cursor;

    QTextCursor tc2 = tc;
    tc2.setPosition(document()->lastBlock().position());
    EDITOR(setTextCursor(tc2));
    EDITOR(ensureCursorVisible());

    int offset = 0;
    const QTextBlock block = document()->findBlockByLineNumber(line);
    if (block.isValid()) {
        const int blockLineCount = block.layout()->lineCount();
        const int lineInBlock = line - block.firstLineNumber();
        if (0 <= lineInBlock && lineInBlock < blockLineCount) {
            QTextLine textLine = block.layout()->lineAt(lineInBlock);
            offset = textLine.textStart();
        } else {
//            QTC_CHECK(false);
        }
    }
    tc2.setPosition(block.position() + offset);
    EDITOR(setTextCursor(tc2));
    EDITOR(ensureCursorVisible());

    EDITOR(setTextCursor(tc));

    m_firstVisibleLine = line;
}

void FakeVimHandler::Private::updateFirstVisibleLine()
{
    const QTextCursor tc = EDITOR(cursorForPosition(QPoint(0,0)));
    m_firstVisibleLine = lineForPosition(tc.position()) - 1;
}

int FakeVimHandler::Private::firstVisibleLine() const
{
    return m_firstVisibleLine;
}

int FakeVimHandler::Private::lastVisibleLine() const
{
    const int line = m_firstVisibleLine + linesOnScreen();
    const QTextBlock block = document()->findBlockByLineNumber(line);
    return block.isValid() ? line : document()->lastBlock().firstLineNumber();
}

int FakeVimHandler::Private::lineOnTop(int count) const
{
    const int scrollOffset = qMax(count - 1, windowScrollOffset());
    const int line = firstVisibleLine();
    return line == 0 ? count - 1 : scrollOffset + line;
}

int FakeVimHandler::Private::lineOnBottom(int count) const
{
    const int scrollOffset = qMax(count - 1, windowScrollOffset());
    const int line = lastVisibleLine();
    return line >= document()->lastBlock().firstLineNumber() ? line - count + 1
                                                             : line - scrollOffset - 1;
}

void FakeVimHandler::Private::scrollUp(int count)
{
    scrollToLine(cursorLine() - cursorLineOnScreen() - count);
}

void FakeVimHandler::Private::updateScrollOffset()
{
    const int line = cursorLine();
    if (line < lineOnTop())
        scrollToLine(qMax(0, line - windowScrollOffset()));
    else if (line > lineOnBottom())
        scrollToLine(firstVisibleLine() + line - lineOnBottom());
}

void FakeVimHandler::Private::alignViewportToCursor(AlignmentFlag align, int line,
    bool moveToNonBlank)
{
    if (line > 0)
        setPosition(firstPositionInLine(line));
    if (moveToNonBlank)
        moveToFirstNonBlankOnLine();

    if (align == Qt::AlignTop)
        scrollUp(- cursorLineOnScreen());
    else if (align == Qt::AlignVCenter)
        scrollUp(linesOnScreen() / 2 - cursorLineOnScreen());
    else if (align == Qt::AlignBottom)
        scrollUp(linesOnScreen() - cursorLineOnScreen() - 1);
}

int FakeVimHandler::Private::lineToBlockNumber(int line) const
{
    return document()->findBlockByLineNumber(line).blockNumber();
}

void FakeVimHandler::Private::setCursorPosition(const CursorPosition &p)
{
    const int firstLine = firstVisibleLine();
    const int firstBlock = lineToBlockNumber(firstLine);
    const int lastBlock = lineToBlockNumber(firstLine + linesOnScreen() - 2);
    bool isLineVisible = firstBlock <= p.line && p.line <= lastBlock;
    setCursorPosition(&m_cursor, p);
    if (!isLineVisible)
        alignViewportToCursor(Qt::AlignVCenter);
}

void FakeVimHandler::Private::setCursorPosition(QTextCursor *tc, const CursorPosition &p)
{
    const int line = qMin(document()->blockCount() - 1, p.line);
    QTextBlock block = document()->findBlockByNumber(line);
    const int column = qMin(p.column, block.length() - 1);
    tc->setPosition(block.position() + column, KeepAnchor);
}

int FakeVimHandler::Private::lastPositionInDocument(bool ignoreMode) const
{
    return document()->characterCount()
        - (ignoreMode || isVisualMode() || isInsertMode() ? 1 : 2);
}

QString FakeVimHandler::Private::selectText(const Range &range) const
{
    QString contents;
    const QString lineEnd = range.rangemode == RangeBlockMode ? QString('\n') : QString();
    QTextCursor tc = m_cursor;
    transformText(range, tc,
        [&tc, &contents, &lineEnd]() { contents.append(tc.selection().toPlainText() + lineEnd); });
    return contents;
}

void FakeVimHandler::Private::yankText(const Range &range, int reg)
{
    const QString text = selectText(range);
    setRegister(reg, text, range.rangemode);

    // If register is not specified or " ...
    if (m_register == '"') {
        // with delete and change commands set register 1 (if text contains more lines) or
        // small delete register -
        if (g.submode == DeleteSubMode || g.submode == ChangeSubMode) {
            if (text.contains('\n'))
                setRegister('1', text, range.rangemode);
            else
                setRegister('-', text, range.rangemode);
        } else {
            // copy to yank register 0 too
            setRegister('0', text, range.rangemode);
        }
    } else if (m_register != '_') {
        // Always copy to " register too (except black hole register).
        setRegister('"', text, range.rangemode);
    }

    const int lines = blockAt(range.endPos).blockNumber()
        - blockAt(range.beginPos).blockNumber() + 1;
    if (lines > 2)
        showMessage(MessageInfo, Tr::tr("%n lines yanked.", nullptr, lines));
}

void FakeVimHandler::Private::transformText(
        const Range &range, QTextCursor &tc, const std::function<void()> &transform) const
{
    switch (range.rangemode) {
        case RangeCharMode: {
            // This can span multiple lines.
            tc.setPosition(range.beginPos, MoveAnchor);
            tc.setPosition(range.endPos, KeepAnchor);
            transform();
            tc.setPosition(range.beginPos);
            break;
        }
        case RangeLineMode:
        case RangeLineModeExclusive: {
            tc.setPosition(range.beginPos, MoveAnchor);
            tc.movePosition(StartOfLine, MoveAnchor);
            tc.setPosition(range.endPos, KeepAnchor);
            tc.movePosition(EndOfLine, KeepAnchor);
            if (range.rangemode != RangeLineModeExclusive) {
                // make sure that complete lines are removed
                // - also at the beginning and at the end of the document
                if (tc.atEnd()) {
                    tc.setPosition(range.beginPos, MoveAnchor);
                    tc.movePosition(StartOfLine, MoveAnchor);
                    if (!tc.atStart()) {
                        // also remove first line if it is the only one
                        tc.movePosition(Left, MoveAnchor, 1);
                        tc.movePosition(EndOfLine, MoveAnchor, 1);
                    }
                    tc.setPosition(range.endPos, KeepAnchor);
                    tc.movePosition(EndOfLine, KeepAnchor);
                } else {
                    tc.movePosition(Right, KeepAnchor, 1);
                }
            }
            const int posAfter = tc.anchor();
            transform();
            tc.setPosition(posAfter);
            break;
        }
        case RangeBlockAndTailMode:
        case RangeBlockMode: {
            int beginColumn = columnAt(range.beginPos);
            int endColumn = columnAt(range.endPos);
            if (endColumn < beginColumn)
                std::swap(beginColumn, endColumn);
            if (range.rangemode == RangeBlockAndTailMode)
                endColumn = INT_MAX - 1;
            QTextBlock block = document()->findBlock(range.beginPos);
            const QTextBlock lastBlock = document()->findBlock(range.endPos);
            while (block.isValid() && block.position() <= lastBlock.position()) {
                int bCol = qMin(beginColumn, block.length() - 1);
                int eCol = qMin(endColumn + 1, block.length() - 1);
                tc.setPosition(block.position() + bCol, MoveAnchor);
                tc.setPosition(block.position() + eCol, KeepAnchor);
                transform();
                block = block.next();
            }
            tc.setPosition(range.beginPos);
            break;
        }
    }
}

void FakeVimHandler::Private::transformText(const Range &range, const Transformation &transform)
{
    beginEditBlock();
    transformText(range, m_cursor,
        [this, &transform] { m_cursor.insertText(transform(m_cursor.selection().toPlainText())); });
    endEditBlock();
    setTargetColumn();
}

void FakeVimHandler::Private::insertText(QTextCursor &tc, const QString &text)
{
  if (hasConfig(ConfigPassKeys)) {
      if (tc.hasSelection() && text.isEmpty()) {
          QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, QString());
          passEventToEditor(event, tc);
      }

      foreach (QChar c, text) {
          QKeyEvent event(QEvent::KeyPress, -1, Qt::NoModifier, QString(c));
          passEventToEditor(event, tc);
      }
  } else {
      tc.insertText(text);
  }
}

void FakeVimHandler::Private::insertText(const Register &reg)
{
    if (reg.rangemode != RangeCharMode) {
        qWarning() << "WRONG INSERT MODE: " << reg.rangemode;
        return;
    }
    setAnchor();
    m_cursor.insertText(reg.contents);
    //dump("AFTER INSERT");
}

void FakeVimHandler::Private::removeText(const Range &range)
{
    transformText(range, [](const QString &) { return QString(); });
}

void FakeVimHandler::Private::downCase(const Range &range)
{
    transformText(range, [](const QString &text) { return text.toLower(); } );
}

void FakeVimHandler::Private::upCase(const Range &range)
{
    transformText(range, [](const QString &text) { return text.toUpper(); } );
}

void FakeVimHandler::Private::invertCase(const Range &range)
{
    transformText(range,
        [] (const QString &text) -> QString {
            QString result = text;
            for (int i = 0; i < result.length(); ++i) {
                QCharRef c = result[i];
                c = c.isUpper() ? c.toLower() : c.toUpper();
            }
            return result;
        });
}

void FakeVimHandler::Private::replaceText(const Range &range, const QString &str)
{
    transformText(range, [&str](const QString &) { return str; } );
}

void FakeVimHandler::Private::pasteText(bool afterCursor)
{
    const QString text = registerContents(m_register);
    const RangeMode rangeMode = registerRangeMode(m_register);

    beginEditBlock();

    // In visual mode paste text only inside selection.
    bool pasteAfter = isVisualMode() ? false : afterCursor;

    if (isVisualMode())
        cutSelectedText('"');

    switch (rangeMode) {
        case RangeCharMode: {
            m_targetColumn = 0;
            const int pos = position() + 1;
            if (pasteAfter && rightDist() > 0)
                moveRight();
            insertText(text.repeated(count()));
            if (text.contains('\n'))
                setPosition(pos);
            else
                moveLeft();
            break;
        }
        case RangeLineMode:
        case RangeLineModeExclusive: {
            QTextCursor tc = m_cursor;
            moveToStartOfLine();
            m_targetColumn = 0;
            bool lastLine = false;
            if (pasteAfter) {
                lastLine = document()->lastBlock() == this->block();
                if (lastLine) {
                    tc.movePosition(EndOfLine, MoveAnchor);
                    tc.insertBlock();
                }
                moveDown();
            }
            const int pos = position();
            if (lastLine)
                insertText(text.repeated(count()).left(text.size() * count() - 1));
            else
                insertText(text.repeated(count()));
            setPosition(pos);
            moveToFirstNonBlankOnLine();
            break;
        }
        case RangeBlockAndTailMode:
        case RangeBlockMode: {
            const int pos = position();
            if (pasteAfter && rightDist() > 0)
                moveRight();
            QTextCursor tc = m_cursor;
            const int col = tc.columnNumber();
            QTextBlock block = tc.block();
            const QStringList lines = text.split('\n');
            for (int i = 0; i < lines.size() - 1; ++i) {
                if (!block.isValid()) {
                    tc.movePosition(EndOfDocument);
                    tc.insertBlock();
                    block = tc.block();
                }

                // resize line
                int length = block.length();
                int begin = block.position();
                if (col >= length) {
                    tc.setPosition(begin + length - 1);
                    tc.insertText(QString(col - length + 1, ' '));
                } else {
                    tc.setPosition(begin + col);
                }

                // insert text
                const QString line = lines.at(i).repeated(count());
                tc.insertText(line);

                // next line
                block = block.next();
            }
            setPosition(pos);
            if (pasteAfter)
                moveRight();
            break;
        }
    }

    endEditBlock();
}

void FakeVimHandler::Private::cutSelectedText(int reg)
{
    pushUndoState();

    bool visualMode = isVisualMode();
    leaveVisualMode();

    Range range = currentRange();
    if (visualMode && g.rangemode == RangeCharMode)
        ++range.endPos;

    if (!reg)
        reg = m_register;

    g.submode = DeleteSubMode;
    yankText(range, reg);
    removeText(range);
    g.submode = NoSubMode;

    if (g.rangemode == RangeLineMode)
        handleStartOfLine();
    else if (g.rangemode == RangeBlockMode)
        setPosition(qMin(position(), anchor()));
}

void FakeVimHandler::Private::joinLines(int count, bool preserveSpace)
{
    int pos = position();
    const int blockNumber = m_cursor.blockNumber();
    for (int i = qMax(count - 2, 0); i >= 0 && blockNumber < document()->blockCount(); --i) {
        moveBehindEndOfLine();
        pos = position();
        setAnchor();
        moveRight();
        if (preserveSpace) {
            removeText(currentRange());
        } else {
            while (characterAtCursor() == ' ' || characterAtCursor() == '\t')
                moveRight();
            m_cursor.insertText(QString(' '));
        }
    }
    setPosition(pos);
}

void FakeVimHandler::Private::insertNewLine()
{
    if ( m_buffer->editBlockLevel <= 1 && hasConfig(ConfigPassKeys) ) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, "\n");
        if (passEventToEditor(event, m_cursor))
            return;
    }

    insertText(QString("\n"));
    insertAutomaticIndentation(true);
}

bool FakeVimHandler::Private::handleInsertInEditor(const Input &input)
{
    if (m_buffer->editBlockLevel > 0 || !hasConfig(ConfigPassKeys))
        return false;

    joinPreviousEditBlock();

    QKeyEvent event(QEvent::KeyPress, input.key(), input.modifiers(), input.text());
    setAnchor();
    if (!passEventToEditor(event, m_cursor))
        return !m_textedit && !m_plaintextedit; // Mark event as handled if it has destroyed editor.

    endEditBlock();

    setTargetColumn();

    return true;
}

bool FakeVimHandler::Private::passEventToEditor(QEvent &event, QTextCursor &tc)
{
    removeEventFilter();
    q->requestDisableBlockSelection();

    setThinCursor();
    EDITOR(setTextCursor(tc));

    bool accepted = QApplication::sendEvent(editor(), &event);
    if (!m_textedit && !m_plaintextedit)
        return false;

    if (accepted)
        tc = editorCursor();

    return accepted;
}

QString FakeVimHandler::Private::lineContents(int line) const
{
    return document()->findBlockByLineNumber(line - 1).text();
}

QString FakeVimHandler::Private::textAt(int from, int to) const
{
    QTextCursor tc(document());
    tc.setPosition(from);
    tc.setPosition(to, KeepAnchor);
    return tc.selectedText().replace(ParagraphSeparator, '\n');
}

void FakeVimHandler::Private::setLineContents(int line, const QString &contents)
{
    QTextBlock block = document()->findBlockByLineNumber(line - 1);
    QTextCursor tc = m_cursor;
    const int begin = block.position();
    const int len = block.length();
    tc.setPosition(begin);
    tc.setPosition(begin + len - 1, KeepAnchor);
    tc.insertText(contents);
}

int FakeVimHandler::Private::blockBoundary(const QString &left,
    const QString &right, bool closing, int count) const
{
    const QString &begin = closing ? left : right;
    const QString &end = closing ? right : left;

    // shift cursor if it is already on opening/closing string
    QTextCursor tc1 = m_cursor;
    int pos = tc1.position();
    int max = document()->characterCount();
    int sz = left.size();
    int from = qMax(pos - sz + 1, 0);
    int to = qMin(pos + sz, max);
    tc1.setPosition(from);
    tc1.setPosition(to, KeepAnchor);
    int i = tc1.selectedText().indexOf(left);
    if (i != -1) {
        // - on opening string:
        tc1.setPosition(from + i + sz);
    } else {
        sz = right.size();
        from = qMax(pos - sz + 1, 0);
        to = qMin(pos + sz, max);
        tc1.setPosition(from);
        tc1.setPosition(to, KeepAnchor);
        i = tc1.selectedText().indexOf(right);
        if (i != -1) {
            // - on closing string:
            tc1.setPosition(from + i);
        } else {
            tc1 = m_cursor;
        }
    }

    QTextCursor tc2 = tc1;
    QTextDocument::FindFlags flags(closing ? 0 : QTextDocument::FindBackward);
    int level = 0;
    int counter = 0;
    while (true) {
        tc2 = document()->find(end, tc2, flags);
        if (tc2.isNull())
            return -1;
        if (!tc1.isNull())
            tc1 = document()->find(begin, tc1, flags);

        while (!tc1.isNull() && (closing ? (tc1 < tc2) : (tc2 < tc1))) {
            ++level;
            tc1 = document()->find(begin, tc1, flags);
        }

        while (level > 0
               && (tc1.isNull() || (closing ? (tc2 < tc1) : (tc1 < tc2)))) {
            --level;
            tc2 = document()->find(end, tc2, flags);
            if (tc2.isNull())
                return -1;
        }

        if (level == 0
            && (tc1.isNull() || (closing ? (tc2 < tc1) : (tc1 < tc2)))) {
            ++counter;
            if (counter >= count)
                break;
        }
    }

    return tc2.position() - end.size();
}

int FakeVimHandler::Private::lineNumber(const QTextBlock &block) const
{
    if (block.isVisible())
        return block.firstLineNumber() + 1;

    // Folded block has line number of the nearest previous visible line.
    QTextBlock block2 = block;
    while (block2.isValid() && !block2.isVisible())
        block2 = block2.previous();
    return block2.firstLineNumber() + 1;
}

int FakeVimHandler::Private::columnAt(int pos) const
{
    return pos - blockAt(pos).position();
}

int FakeVimHandler::Private::blockNumberAt(int pos) const
{
    return blockAt(pos).blockNumber();
}

QTextBlock FakeVimHandler::Private::blockAt(int pos) const
{
    return document()->findBlock(pos);
}

QTextBlock FakeVimHandler::Private::nextLine(const QTextBlock &block) const
{
    return blockAt(block.position() + block.length());
}

QTextBlock FakeVimHandler::Private::previousLine(const QTextBlock &block) const
{
    return blockAt(block.position() - 1);
}

int FakeVimHandler::Private::firstPositionInLine(int line, bool onlyVisibleLines) const
{
    QTextBlock block = onlyVisibleLines ? document()->findBlockByLineNumber(line - 1)
        : document()->findBlockByNumber(line - 1);
    return block.position();
}

int FakeVimHandler::Private::lastPositionInLine(int line, bool onlyVisibleLines) const
{
    QTextBlock block;
    if (onlyVisibleLines) {
        block = document()->findBlockByLineNumber(line - 1);
        // respect folds and wrapped lines
        do {
            block = nextLine(block);
        } while (block.isValid() && !block.isVisible());
        if (block.isValid()) {
            if (line > 0)
                block = block.previous();
        } else {
            block = document()->lastBlock();
        }
    } else {
        block = document()->findBlockByNumber(line - 1);
    }

    const int position = block.position() + block.length() - 1;
    if (block.length() > 1 && !isVisualMode() && !isInsertMode())
        return position - 1;
    return position;
}

int FakeVimHandler::Private::lineForPosition(int pos) const
{
    const QTextBlock block = blockAt(pos);
    if (!block.isValid())
        return 0;
    const int positionInBlock = pos - block.position();
    const int lineNumberInBlock = block.layout()->lineForTextPosition(positionInBlock).lineNumber();
    return block.firstLineNumber() + lineNumberInBlock + 1;
}

void FakeVimHandler::Private::toggleVisualMode(VisualMode visualMode)
{
    if (visualMode == g.visualMode) {
        leaveVisualMode();
    } else {
        m_positionPastEnd = false;
        m_anchorPastEnd = false;
        g.visualMode = visualMode;
        m_buffer->lastVisualMode = visualMode;
    }
}

void FakeVimHandler::Private::leaveVisualMode()
{
    if (!isVisualMode())
        return;

    if (isVisualLineMode()) {
        g.rangemode = RangeLineMode;
        g.movetype = MoveLineWise;
    } else if (isVisualCharMode()) {
        g.rangemode = RangeCharMode;
        g.movetype = MoveInclusive;
    } else if (isVisualBlockMode()) {
        g.rangemode = m_visualTargetColumn == -1 ? RangeBlockAndTailMode : RangeBlockMode;
        g.movetype = MoveInclusive;
    }

    g.visualMode = NoVisualMode;
}

void FakeVimHandler::Private::saveLastVisualMode()
{
    if (isVisualMode() && g.mode == CommandMode && g.submode == NoSubMode) {
        setMark('<', markLessPosition());
        setMark('>', markGreaterPosition());
        m_buffer->lastVisualModeInverted = anchor() > position();
        m_buffer->lastVisualMode = g.visualMode;
    }
}

QWidget *FakeVimHandler::Private::editor() const
{
    return m_textedit
        ? static_cast<QWidget *>(m_textedit)
        : static_cast<QWidget *>(m_plaintextedit);
}

void FakeVimHandler::Private::joinPreviousEditBlock()
{
    UNDO_DEBUG("JOIN");
    if (m_buffer->breakEditBlock) {
        beginEditBlock();
        QTextCursor tc(m_cursor);
        tc.setPosition(tc.position());
        tc.beginEditBlock();
        tc.insertText("X");
        tc.deletePreviousChar();
        tc.endEditBlock();
        m_buffer->breakEditBlock = false;
    } else {
        if (m_buffer->editBlockLevel == 0 && !m_buffer->undo.empty())
            m_buffer->undoState = m_buffer->undo.pop();
        beginEditBlock();
    }
}

void FakeVimHandler::Private::beginEditBlock(bool largeEditBlock)
{
    UNDO_DEBUG("BEGIN EDIT BLOCK" << m_buffer->editBlockLevel + 1);
    if (!largeEditBlock && !m_buffer->undoState.isValid())
        pushUndoState(false);
    if (m_buffer->editBlockLevel == 0)
        m_buffer->breakEditBlock = true;
    ++m_buffer->editBlockLevel;
}

void FakeVimHandler::Private::endEditBlock()
{
    UNDO_DEBUG("END EDIT BLOCK" << m_buffer->editBlockLevel);
    if (m_buffer->editBlockLevel <= 0) {
        qWarning("beginEditBlock() not called before endEditBlock()!");
        return;
    }
    --m_buffer->editBlockLevel;
    if (m_buffer->editBlockLevel == 0 && m_buffer->undoState.isValid()) {
        m_buffer->undo.push(m_buffer->undoState);
        m_buffer->undoState = State();
    }
    if (m_buffer->editBlockLevel == 0)
        m_buffer->breakEditBlock = false;
}

void FakeVimHandler::Private::onContentsChanged(int position, int charsRemoved, int charsAdded)
{
    // Record inserted and deleted text in insert mode.
    if (isInsertMode() && (charsAdded > 0 || charsRemoved > 0) && canModifyBufferData()) {
        BufferData::InsertState &insertState = m_buffer->insertState;
        const int oldPosition = insertState.pos2;
        if (!isInsertStateValid()) {
            insertState.pos1 = oldPosition;
            g.dotCommand = "i";
            resetCount();
        }

        // Ignore changes outside inserted text (e.g. renaming other occurrences of a variable).
        if (position + charsRemoved >= insertState.pos1 && position <= insertState.pos2) {
            if (charsRemoved > 0) {
                // Assume that in a manual edit operation a text can be removed only
                // in front of cursor (<DELETE>) or behind it (<BACKSPACE>).

                // If the recorded amount of backspace/delete keys doesn't correspond with
                // number of removed characters, assume that the document has been changed
                // externally and invalidate current insert state.

                const bool wholeDocumentChanged =
                        charsRemoved > 1
                        && charsAdded > 0
                        && charsAdded + 1 == document()->characterCount();

                if (position < insertState.pos1) {
                    // <BACKSPACE>
                    const int backspaceCount = insertState.pos1 - position;
                    if (backspaceCount != charsRemoved || (oldPosition == charsRemoved && wholeDocumentChanged)) {
                        invalidateInsertState();
                    } else {
                        const QString inserted = textAt(position, oldPosition);
                        const QString removed = insertState.textBeforeCursor.right(backspaceCount);
                        // Ignore backspaces if same text was just inserted.
                        if ( !inserted.endsWith(removed) ) {
                            insertState.backspaces += backspaceCount;
                            insertState.pos1 = position;
                            insertState.pos2 = qMax(position, insertState.pos2 - backspaceCount);
                        }
                    }
                } else if (position + charsRemoved > insertState.pos2) {
                    // <DELETE>
                    const int deleteCount = position + charsRemoved - insertState.pos2;
                    if (deleteCount != charsRemoved || (oldPosition == 0 && wholeDocumentChanged))
                        invalidateInsertState();
                    else
                        insertState.deletes += deleteCount;
                }
            } else if (charsAdded > 0 && insertState.insertingSpaces) {
                for (int i = position; i < position + charsAdded; ++i) {
                    const QChar c = characterAt(i);
                    if (c.unicode() == ' ' || c.unicode() == '\t')
                        insertState.spaces.insert(i);
                }
            }

            const int newPosition = position + charsAdded;
            insertState.pos2 = qMax(insertState.pos2 + charsAdded - charsRemoved, newPosition);
            insertState.textBeforeCursor = textAt(block().position(), newPosition);
        }
    }

    if (!m_highlighted.isEmpty())
        q->highlightMatches(m_highlighted);
}

void FakeVimHandler::Private::onCursorPositionChanged()
{
    if (!m_inFakeVim) {
        m_cursorNeedsUpdate = true;

        // Selecting text with mouse disables the thick cursor so it's more obvious
        // that extra character under cursor is not selected when moving text around or
        // making operations on text outside FakeVim mode.
        setThinCursor(g.mode == InsertMode || editorCursor().hasSelection());
    }
}

void FakeVimHandler::Private::onUndoCommandAdded()
{
    if (!canModifyBufferData())
        return;

    // Undo commands removed?
    UNDO_DEBUG("Undo added" << "previous: REV" << m_buffer->lastRevision);
    if (m_buffer->lastRevision >= revision()) {
        UNDO_DEBUG("UNDO REMOVED!");
        const int removed = m_buffer->lastRevision - revision();
        for (int i = m_buffer->undo.size() - 1; i >= 0; --i) {
            if ((m_buffer->undo[i].revision -= removed) < 0) {
                m_buffer->undo.remove(0, i + 1);
                break;
            }
        }
    }

    m_buffer->redo.clear();
    // External change while FakeVim disabled.
    if (m_buffer->editBlockLevel == 0 && !m_buffer->undo.isEmpty() && !isInsertMode())
        m_buffer->undo.push(State());
}

void FakeVimHandler::Private::onInputTimeout()
{
    enterFakeVim();
    EventResult result = handleKey(Input());
    leaveFakeVim(result);
}

void FakeVimHandler::Private::onFixCursorTimeout()
{
    if (editor())
        fixExternalCursorPosition(editor()->hasFocus() && !isCommandLineMode());
}

char FakeVimHandler::Private::currentModeCode() const
{
    if (g.mode == ExMode)
        return 'c';
    else if (isVisualMode())
        return 'v';
    else if (isOperatorPending())
        return 'o';
    else if (g.mode == CommandMode)
        return 'n';
    else if (g.submode != NoSubMode)
        return ' ';
    else
        return 'i';
}

void FakeVimHandler::Private::undoRedo(bool undo)
{
    UNDO_DEBUG((undo ? "UNDO" : "REDO"));

    // FIXME: That's only an approximaxtion. The real solution might
    // be to store marks and old userData with QTextBlock setUserData
    // and retrieve them afterward.
    QStack<State> &stack = undo ? m_buffer->undo : m_buffer->redo;
    QStack<State> &stack2 = undo ? m_buffer->redo : m_buffer->undo;

    State state = m_buffer->undoState.isValid() ? m_buffer->undoState
                                        : !stack.empty() ? stack.pop() : State();

    CursorPosition lastPos(m_cursor);
    if (undo ? !document()->isUndoAvailable() : !document()->isRedoAvailable()) {
        const QString msg = undo ? Tr::tr("Already at oldest change.")
            : Tr::tr("Already at newest change.");
        showMessage(MessageInfo, msg);
        UNDO_DEBUG(msg);
        return;
    }
    clearMessage();

    ++m_buffer->editBlockLevel;

    // Do undo/redo [count] times to reach previous revision.
    const int previousRevision = revision();
    if (undo) {
        do {
            EDITOR(undo());
        } while (document()->isUndoAvailable() && state.revision >= 0 && state.revision < revision());
    } else {
        do {
            EDITOR(redo());
        } while (document()->isRedoAvailable() && state.revision > revision());
    }

    --m_buffer->editBlockLevel;

    if (state.isValid()) {
        Marks marks = m_buffer->marks;
        marks.swap(state.marks);
        updateMarks(marks);
        m_buffer->lastVisualMode = state.lastVisualMode;
        m_buffer->lastVisualModeInverted = state.lastVisualModeInverted;
        setMark('.', state.position);
        setMark('\'', lastPos);
        setMark('`', lastPos);
        setCursorPosition(state.position);
        setAnchor();
        state.revision = previousRevision;
    } else {
        updateFirstVisibleLine();
        pullCursor();
    }
    stack2.push(state);

    setTargetColumn();
    if (atEndOfLine())
        moveLeft();

    UNDO_DEBUG((undo ? "UNDONE" : "REDONE"));
}

void FakeVimHandler::Private::undo()
{
    undoRedo(true);
}

void FakeVimHandler::Private::redo()
{
    undoRedo(false);
}

void FakeVimHandler::Private::updateCursorShape()
{
    setThinCursor(
        g.mode == InsertMode
        || isVisualLineMode()
        || isVisualBlockMode()
        || isCommandLineMode()
        || !editor()->hasFocus());
}

void FakeVimHandler::Private::setThinCursor(bool enable)
{
    EDITOR(setOverwriteMode(!enable));
}

bool FakeVimHandler::Private::hasThinCursor() const
{
    return !EDITOR(overwriteMode());
}

void FakeVimHandler::Private::enterReplaceMode()
{
    enterInsertOrReplaceMode(ReplaceMode);
}

void FakeVimHandler::Private::enterInsertMode()
{
    enterInsertOrReplaceMode(InsertMode);
}

void FakeVimHandler::Private::enterInsertOrReplaceMode(Mode mode)
{
    if (mode != InsertMode && mode != ReplaceMode) {
        qWarning("Unexpected mode");
        return;
    }
    if (g.mode == mode)
        return;

    g.mode = mode;

    if (g.returnToMode == mode) {
        // Returning to insert mode after <C-O>.
        clearCurrentMode();
        moveToTargetColumn();
        invalidateInsertState();
    } else {
        // Entering insert mode from command mode.
        if (mode == InsertMode) {
            // m_targetColumn shouldn't be -1 (end of line).
            if (m_targetColumn == -1)
                setTargetColumn();
        }

        g.submode = NoSubMode;
        g.subsubmode = NoSubSubMode;
        g.returnToMode = mode;
        clearLastInsertion();
    }
}

void FakeVimHandler::Private::enterVisualInsertMode(QChar command)
{
    if (isVisualBlockMode()) {
        bool append = command == 'A';
        bool change = command == 's' || command == 'c';

        leaveVisualMode();

        const CursorPosition lastAnchor = markLessPosition();
        const CursorPosition lastPosition = markGreaterPosition();
        CursorPosition pos(lastAnchor.line,
            append ? qMax(lastPosition.column, lastAnchor.column) + 1
                   : qMin(lastPosition.column, lastAnchor.column));

        if (append) {
            m_visualBlockInsert = m_visualTargetColumn == -1 ? AppendToEndOfLineBlockInsertMode
                                                             : AppendBlockInsertMode;
        } else if (change) {
            m_visualBlockInsert = ChangeBlockInsertMode;
            beginEditBlock();
            cutSelectedText();
            endEditBlock();
        } else {
            m_visualBlockInsert = InsertBlockInsertMode;
        }

        setCursorPosition(pos);
        if (m_visualBlockInsert == AppendToEndOfLineBlockInsertMode)
            moveBehindEndOfLine();
    } else {
        m_visualBlockInsert = NoneBlockInsertMode;
        leaveVisualMode();
        if (command == 'I') {
            if (lineForPosition(anchor()) <= lineForPosition(position())) {
                setPosition(qMin(anchor(), position()));
                moveToStartOfLine();
            }
        } else if (command == 'A') {
            if (lineForPosition(anchor()) <= lineForPosition(position())) {
                setPosition(position());
                moveRight(qMin(rightDist(), 1));
            } else {
                setPosition(anchor());
                moveToStartOfLine();
            }
        }
    }

    setAnchor();
    if (m_visualBlockInsert != ChangeBlockInsertMode)
        breakEditBlock();
    enterInsertMode();
}

void FakeVimHandler::Private::enterCommandMode(Mode returnToMode)
{
    if (g.isRecording && isCommandLineMode())
        record(Input(Key_Escape, NoModifier));

    if (isNoVisualMode()) {
        if (atEndOfLine()) {
            m_cursor.movePosition(Left, KeepAnchor);
            if (m_targetColumn != -1)
                setTargetColumn();
        }
        setAnchor();
    }

    g.mode = CommandMode;
    clearCurrentMode();
    g.returnToMode = returnToMode;
    m_positionPastEnd = false;
    m_anchorPastEnd = false;
}

void FakeVimHandler::Private::enterExMode(const QString &contents)
{
    g.currentMessage.clear();
    g.commandBuffer.clear();
    if (isVisualMode())
        g.commandBuffer.setContents(QString("'<,'>") + contents, contents.size() + 5);
    else
        g.commandBuffer.setContents(contents, contents.size());
    g.mode = ExMode;
    g.submode = NoSubMode;
    g.subsubmode = NoSubSubMode;
    unfocus();
}

void FakeVimHandler::Private::recordJump(int position)
{
    CursorPosition pos = position >= 0 ? CursorPosition(document(), position)
                                       : CursorPosition(m_cursor);
    setMark('\'', pos);
    setMark('`', pos);
    if (m_buffer->jumpListUndo.isEmpty() || m_buffer->jumpListUndo.top() != pos)
        m_buffer->jumpListUndo.push(pos);
    m_buffer->jumpListRedo.clear();
    UNDO_DEBUG("jumps: " << m_buffer->jumpListUndo);
}

void FakeVimHandler::Private::jump(int distance)
{
    QStack<CursorPosition> &from = (distance > 0) ? m_buffer->jumpListRedo : m_buffer->jumpListUndo;
    QStack<CursorPosition> &to   = (distance > 0) ? m_buffer->jumpListUndo : m_buffer->jumpListRedo;
    int len = qMin(qAbs(distance), from.size());
    CursorPosition m(m_cursor);
    setMark('\'', m);
    setMark('`', m);
    for (int i = 0; i < len; ++i) {
        to.push(m);
        setCursorPosition(from.top());
        from.pop();
    }
    setTargetColumn();
}

Column FakeVimHandler::Private::indentation(const QString &line) const
{
    int ts = config(ConfigTabStop).toInt();
    int physical = 0;
    int logical = 0;
    int n = line.size();
    while (physical < n) {
        QChar c = line.at(physical);
        if (c == ' ')
            ++logical;
        else if (c == '\t')
            logical += ts - logical % ts;
        else
            break;
        ++physical;
    }
    return Column(physical, logical);
}

QString FakeVimHandler::Private::tabExpand(int n) const
{
    int ts = config(ConfigTabStop).toInt();
    if (hasConfig(ConfigExpandTab) || ts < 1)
        return QString(n, ' ');
    return QString(n / ts, '\t')
         + QString(n % ts, ' ');
}

void FakeVimHandler::Private::insertAutomaticIndentation(bool goingDown, bool forceAutoIndent)
{
    if (!forceAutoIndent && !hasConfig(ConfigAutoIndent) && !hasConfig(ConfigSmartIndent))
        return;

    if (hasConfig(ConfigSmartIndent)) {
        QTextBlock bl = block();
        Range range(bl.position(), bl.position());
        indentText(range, '\n');
    } else {
        QTextBlock bl = goingDown ? block().previous() : block().next();
        QString text = bl.text();
        int pos = 0;
        int n = text.size();
        while (pos < n && text.at(pos).isSpace())
            ++pos;
        text.truncate(pos);
        // FIXME: handle 'smartindent' and 'cindent'
        insertText(text);
    }
}

void FakeVimHandler::Private::handleStartOfLine()
{
    if (hasConfig(ConfigStartOfLine))
        moveToFirstNonBlankOnLine();
}

void FakeVimHandler::Private::replay(const QString &command, int repeat)
{
    if (repeat <= 0)
        return;

    //qDebug() << "REPLAY: " << quoteUnprintable(command);
    clearCurrentMode();
    Inputs inputs(command);
    for (int i = 0; i < repeat; ++i) {
        foreach (const Input &in, inputs) {
            if (handleDefaultKey(in) != EventHandled)
                return;
        }
    }
}

QString FakeVimHandler::Private::visualDotCommand() const
{
    QTextCursor start(m_cursor);
    QTextCursor end(start);
    end.setPosition(end.anchor());

    QString command;

    if (isVisualCharMode())
        command = "v";
    else if (isVisualLineMode())
        command = "V";
    else if (isVisualBlockMode())
        command = "<c-v>";
    else
        return QString();

    const int down = qAbs(start.blockNumber() - end.blockNumber());
    if (down != 0)
        command.append(QString("%1j").arg(down));

    const int right = start.positionInBlock() - end.positionInBlock();
    if (right != 0) {
        command.append(QString::number(qAbs(right)));
        command.append(QLatin1Char(right < 0 && isVisualBlockMode() ? 'h' : 'l'));
    }

    return command;
}

void FakeVimHandler::Private::selectTextObject(bool simple, bool inner)
{
    const int position1 = this->position();
    const int anchor1 = this->anchor();
    bool setupAnchor = (position1 == anchor1);
    bool forward = anchor1 <= position1;
    const int repeat = count();

    // set anchor if not already set
    if (setupAnchor) {
        // Select nothing with 'inner' on empty line.
        if (inner && atEmptyLine() && repeat == 1) {
            g.movetype = MoveExclusive;
            return;
        }
        moveToBoundaryStart(1, simple, false);
        setAnchor();
    } else if (forward) {
        moveToNextCharacter();
    } else {
        moveToPreviousCharacter();
    }

    if (inner) {
        moveToBoundaryEnd(repeat, simple);
    } else {
        const int direction = forward ? 1 : -1;
        for (int i = 0; i < repeat; ++i) {
            // select leading spaces
            bool leadingSpace = characterAtCursor().isSpace();
            if (leadingSpace) {
                if (forward)
                    moveToNextBoundaryStart(1, simple);
                else
                    moveToNextBoundaryEnd(1, simple, false);
            }

            // select word
            if (forward)
                moveToWordEnd(1, simple);
            else
                moveToWordStart(1, simple, false);

            // select trailing spaces if no leading space
            QChar afterCursor = characterAt(position() + direction);
            if (!leadingSpace && afterCursor.isSpace() && afterCursor != ParagraphSeparator
                && !atBlockStart()) {
                if (forward)
                    moveToNextBoundaryEnd(1, simple);
                else
                    moveToNextBoundaryStart(1, simple, false);
            }

            // if there are no trailing spaces in selection select all leading spaces
            // after previous character
            if (setupAnchor && (!characterAtCursor().isSpace() || atBlockEnd())) {
                int min = block().position();
                int pos = anchor();
                while (pos >= min && characterAt(--pos).isSpace()) {}
                if (pos >= min)
                    setAnchorAndPosition(pos + 1, position());
            }

            if (i + 1 < repeat) {
                if (forward)
                    moveToNextCharacter();
                else
                    moveToPreviousCharacter();
            }
        }
    }

    if (inner) {
        g.movetype = MoveInclusive;
    } else {
        g.movetype = MoveExclusive;
        if (isNoVisualMode())
            moveToNextCharacter();
        else if (isVisualLineMode())
            g.visualMode = VisualCharMode;
    }

    setTargetColumn();
}

void FakeVimHandler::Private::selectWordTextObject(bool inner)
{
    selectTextObject(false, inner);
}

void FakeVimHandler::Private::selectWORDTextObject(bool inner)
{
    selectTextObject(true, inner);
}

void FakeVimHandler::Private::selectSentenceTextObject(bool inner)
{
    Q_UNUSED(inner)
}

void FakeVimHandler::Private::selectParagraphTextObject(bool inner)
{
    const QTextCursor oldCursor = m_cursor;
    const VisualMode oldVisualMode = g.visualMode;

    const int anchorBlock = blockNumberAt(anchor());
    const int positionBlock = blockNumberAt(position());
    const bool setupAnchor = anchorBlock == positionBlock;
    int repeat = count();

    // If anchor and position are in the same block,
    // start line selection at beginning of current paragraph.
    if (setupAnchor) {
        moveToParagraphStartOrEnd(-1);
        setAnchor();

        if (!isVisualLineMode() && isVisualMode())
            toggleVisualMode(VisualLineMode);
    }

    const bool forward = anchor() <= position();
    const int d = forward ? 1 : -1;

    bool startsAtParagraph = !atEmptyLine(position());

    moveToParagraphStartOrEnd(d);

    // If selection already changed, decreate count.
    if ((setupAnchor && g.submode != NoSubMode)
        || oldVisualMode != g.visualMode
        || m_cursor != oldCursor)
    {
        --repeat;
        if (!inner) {
            moveDown(d);
            moveToParagraphStartOrEnd(d);
            startsAtParagraph = !startsAtParagraph;
        }
    }

    if (repeat > 0) {
        bool isCountEven = repeat % 2 == 0;
        bool endsOnParagraph =
                inner ? isCountEven == startsAtParagraph : startsAtParagraph;

        if (inner) {
            repeat = repeat / 2;
            if (!isCountEven || endsOnParagraph)
                ++repeat;
        } else {
            if (endsOnParagraph)
                ++repeat;
        }

        if (!moveToNextParagraph(d * repeat)) {
            m_cursor = oldCursor;
            g.visualMode = oldVisualMode;
            return;
        }

        if (endsOnParagraph && atEmptyLine())
            moveUp(d);
        else
            moveToParagraphStartOrEnd(d);
    }

    if (!inner && setupAnchor && !atEmptyLine() && !atEmptyLine(anchor())) {
        // If position cannot select empty lines, try to select them with anchor.
        setAnchorAndPosition(position(), anchor());
        moveToNextParagraph(-d);
        moveToParagraphStartOrEnd(-d);
        setAnchorAndPosition(position(), anchor());
    }

    recordJump(oldCursor.position());
    setTargetColumn();
    g.movetype = MoveLineWise;
}

bool FakeVimHandler::Private::selectBlockTextObject(bool inner,
    QChar left, QChar right)
{
    int p1 = blockBoundary(left, right, false, count());
    if (p1 == -1)
        return false;

    int p2 = blockBoundary(left, right, true, count());
    if (p2 == -1)
        return false;

    g.movetype = MoveExclusive;

    if (inner) {
        p1 += 1;
        bool moveStart = characterAt(p1) == ParagraphSeparator;
        bool moveEnd = isFirstNonBlankOnLine(p2);
        if (moveStart)
            ++p1;
        if (moveEnd)
            p2 = blockAt(p2).position() - 1;
        if (moveStart && moveEnd)
            g.movetype = MoveLineWise;
    } else {
        p2 += 1;
    }

    if (isVisualMode())
        --p2;

    setAnchorAndPosition(p1, p2);

    return true;
}

bool FakeVimHandler::Private::changeNumberTextObject(int count)
{
    const QTextBlock block = this->block();
    const QString lineText = block.text();
    const int posMin = m_cursor.positionInBlock() + 1;

    // find first decimal, hexadecimal or octal number under or after cursor position
    QRegularExpression re("(0[xX])(0*[0-9a-fA-F]+)|(0)(0*[0-7]+)(?=\\D|$)|(\\d+)");
    QRegularExpressionMatch m;
    QRegularExpressionMatchIterator it = re.globalMatch(lineText);
    while (true) {
        if (!it.hasNext())
            return false;
        m = it.next();
        if (m.capturedEnd() >= posMin)
            break;
    }

    int pos = m.capturedStart();
    int len = m.capturedLength();
    QString prefix = m.captured(1) + m.captured(3);
    bool hex = prefix.length() >= 2 && (prefix[1].toLower() == 'x');
    bool octal = !hex && !prefix.isEmpty();
    const QString num = hex ? m.captured(2) : octal ? m.captured(4) : m.captured(5);

    // parse value
    bool ok;
    int base = hex ? 16 : octal ? 8 : 10;
    qlonglong value = 0;  // decimal value
    qlonglong uvalue = 0; // hexadecimal or octal value (only unsigned)
    if (hex || octal)
        uvalue = num.toULongLong(&ok, base);
    else
        value = num.toLongLong(&ok, base);
    if (!ok) {
        qWarning() << "Cannot parse number:" << num << "base:" << base;
        return false;
    }

    // negative decimal number
    if (!octal && !hex && pos > 0 && lineText[pos - 1] == '-') {
        value = -value;
        --pos;
        ++len;
    }

    // result to string
    QString repl;
    if (hex || octal)
        repl = QString::number(uvalue + count, base);
    else
        repl = QString::number(value + count, base);

    // convert hexadecimal number to upper-case if last letter was upper-case
    if (hex) {
        const int lastLetter = num.lastIndexOf(QRegularExpression("[a-fA-F]"));
        if (lastLetter != -1 && num[lastLetter].isUpper())
            repl = repl.toUpper();
    }

    // preserve leading zeroes
    if ((octal || hex) && repl.size() < num.size())
        prefix.append(QString("0").repeated(num.size() - repl.size()));
    repl.prepend(prefix);

    pos += block.position();
    pushUndoState();
    setAnchorAndPosition(pos, pos + len);
    replaceText(currentRange(), repl);
    setPosition(pos + repl.size() - 1);

    return true;
}

bool FakeVimHandler::Private::selectQuotedStringTextObject(bool inner,
    const QString &quote)
{
    QTextCursor tc = m_cursor;
    int sz = quote.size();

    QTextCursor tc1;
    QTextCursor tc2(document());
    while (tc2 <= tc) {
        tc1 = document()->find(quote, tc2);
        if (tc1.isNull())
            return false;
        tc2 = document()->find(quote, tc1);
        if (tc2.isNull())
            return false;
    }

    int p1 = tc1.position();
    int p2 = tc2.position();
    if (inner) {
        p2 = qMax(p1, p2 - sz);
        if (characterAt(p1) == ParagraphSeparator)
            ++p1;
    } else {
        p1 -= sz;
        p2 -= sz - 1;
    }

    if (isVisualMode())
        --p2;

    setAnchorAndPosition(p1, p2);
    g.movetype = MoveExclusive;

    return true;
}

Mark FakeVimHandler::Private::mark(QChar code) const
{
    if (isVisualMode()) {
        if (code == '<')
            return CursorPosition(document(), qMin(anchor(), position()));
        if (code == '>')
            return CursorPosition(document(), qMax(anchor(), position()));
    }

    if (code.isUpper())
        return g.marks.value(code);

    return m_buffer->marks.value(code);
}

void FakeVimHandler::Private::setMark(QChar code, CursorPosition position)
{
    if (code.isUpper())
        g.marks[code] = Mark(position, m_currentFileName);
    else
        m_buffer->marks[code] = Mark(position);
}

bool FakeVimHandler::Private::jumpToMark(QChar mark, bool backTickMode)
{
    Mark m = this->mark(mark);
    if (!m.isValid()) {
        showMessage(MessageError, msgMarkNotSet(mark));
        return false;
    }
    if (!m.isLocal(m_currentFileName)) {
        q->requestJumpToGlobalMark(mark, backTickMode, m.fileName());
        return false;
    }

    if ((mark == '\'' || mark == '`') && !m_buffer->jumpListUndo.isEmpty())
        m_buffer->jumpListUndo.pop();
    recordJump();
    setCursorPosition(m.position(document()));
    if (!backTickMode)
        moveToFirstNonBlankOnLine();
    if (g.submode == NoSubMode)
        setAnchor();
    setTargetColumn();

    return true;
}

void FakeVimHandler::Private::updateMarks(const Marks &newMarks)
{
    for (auto it = newMarks.cbegin(), end = newMarks.cend(); it != end; ++it)
        m_buffer->marks[it.key()] = it.value();
}

RangeMode FakeVimHandler::Private::registerRangeMode(int reg) const
{
    bool isClipboard;
    bool isSelection;
    getRegisterType(&reg, &isClipboard, &isSelection);

    if (isClipboard || isSelection) {
        QClipboard *clipboard = QApplication::clipboard();
        QClipboard::Mode mode = isClipboard ? QClipboard::Clipboard : QClipboard::Selection;

        // Use range mode from Vim's clipboard data if available.
        const QMimeData *data = clipboard->mimeData(mode);
        if (data && data->hasFormat(vimMimeText)) {
            QByteArray bytes = data->data(vimMimeText);
            if (bytes.length() > 0)
                return static_cast<RangeMode>(bytes.at(0));
        }

        // If register content is clipboard:
        //  - return RangeLineMode if text ends with new line char,
        //  - return RangeCharMode otherwise.
        QString text = clipboard->text(mode);
        return (text.endsWith('\n') || text.endsWith('\r')) ? RangeLineMode : RangeCharMode;
    }

    return g.registers[reg].rangemode;
}

void FakeVimHandler::Private::setRegister(int reg, const QString &contents, RangeMode mode)
{
    bool copyToClipboard;
    bool copyToSelection;
    bool append;
    getRegisterType(&reg, &copyToClipboard, &copyToSelection, &append);

    QString contents2 = contents;
    if ((mode == RangeLineMode || mode == RangeLineModeExclusive)
            && !contents2.endsWith('\n'))
    {
        contents2.append('\n');
    }

    if (copyToClipboard || copyToSelection) {
        if (copyToClipboard)
            setClipboardData(contents2, mode, QClipboard::Clipboard);
        if (copyToSelection)
            setClipboardData(contents2, mode, QClipboard::Selection);
    } else {
        if (append)
            g.registers[reg].contents.append(contents2);
        else
            g.registers[reg].contents = contents2;
        g.registers[reg].rangemode = mode;
    }
}

QString FakeVimHandler::Private::registerContents(int reg) const
{
    bool copyFromClipboard;
    bool copyFromSelection;
    getRegisterType(&reg, &copyFromClipboard, &copyFromSelection);

    if (copyFromClipboard || copyFromSelection) {
        QClipboard *clipboard = QApplication::clipboard();
        if (copyFromClipboard)
            return clipboard->text(QClipboard::Clipboard);
        if (copyFromSelection)
            return clipboard->text(QClipboard::Selection);
    }

    return g.registers[reg].contents;
}

void FakeVimHandler::Private::getRegisterType(int *reg, bool *isClipboard, bool *isSelection, bool *append) const
{
    bool clipboard = false;
    bool selection = false;

    // If register is uppercase, append content to lower case register on yank/delete.
    const QChar c(*reg);
    if (append != nullptr)
        *append = c.isUpper();
    if (c.isUpper())
        *reg = c.toLower().unicode();

    if (c == '"') {
        QStringList list = config(ConfigClipboard).toString().split(',');
        clipboard = list.contains("unnamedplus");
        selection = list.contains("unnamed");
    } else if (c == '+') {
        clipboard = true;
    } else if (c == '*') {
        selection = true;
    }

    // selection (primary) is clipboard on systems without selection support
    if (selection && !QApplication::clipboard()->supportsSelection()) {
        clipboard = true;
        selection = false;
    }

    if (isClipboard != nullptr)
        *isClipboard = clipboard;
    if (isSelection != nullptr)
        *isSelection = selection;
}

///////////////////////////////////////////////////////////////////////
//
// FakeVimHandler
//
///////////////////////////////////////////////////////////////////////

FakeVimHandler::FakeVimHandler(QWidget *widget, QObject *parent)
    : QObject(parent), d(new Private(this, widget))
{}

FakeVimHandler::~FakeVimHandler()
{
    delete d;
}

// gracefully handle that the parent editor is deleted
void FakeVimHandler::disconnectFromEditor()
{
    d->m_textedit = nullptr;
    d->m_plaintextedit = nullptr;
}

void FakeVimHandler::updateGlobalMarksFilenames(const QString &oldFileName, const QString &newFileName)
{
    for (int i = 0; i < Private::g.marks.size(); ++i) {
        Mark &mark = Private::g.marks[i];
        if (mark.fileName() == oldFileName)
            mark.setFileName(newFileName);
    }
}

bool FakeVimHandler::eventFilter(QObject *ob, QEvent *ev)
{
#ifndef FAKEVIM_STANDALONE
    if (!theFakeVimSetting(ConfigUseFakeVim)->value().toBool())
        return QObject::eventFilter(ob, ev);
#endif

    if (ev->type() == QEvent::Shortcut) {
        d->passShortcuts(false);
        return false;
    }

    if (ev->type() == QEvent::KeyPress &&
        (ob == d->editor()
         || (Private::g.mode == ExMode || Private::g.subsubmode == SearchSubSubMode))) {
        auto kev = static_cast<QKeyEvent *>(ev);
        KEY_DEBUG("KEYPRESS" << kev->key() << kev->text() << QChar(kev->key()));
        if ( kev->key() == Key_Escape && !d->wantsOverride(kev) )
            return false;
        EventResult res = d->handleEvent(kev);
        //if (Private::g.mode == InsertMode)
        //    completionRequested();
        // returning false core the app see it
        //KEY_DEBUG("HANDLED CODE:" << res);
        //return res != EventPassedToCore;
        //return true;
        return res == EventHandled || res == EventCancelled;
    }

    if (ev->type() == QEvent::ShortcutOverride && (ob == d->editor()
         || (Private::g.mode == ExMode || Private::g.subsubmode == SearchSubSubMode))) {
        auto kev = static_cast<QKeyEvent *>(ev);
        if (d->wantsOverride(kev)) {
            KEY_DEBUG("OVERRIDING SHORTCUT" << kev->key());
            ev->accept(); // accepting means "don't run the shortcuts"
            return true;
        }
        KEY_DEBUG("NO SHORTCUT OVERRIDE" << kev->key());
        return true;
    }

    if (ev->type() == QEvent::FocusOut && ob == d->editor()) {
        d->unfocus();
        return false;
    }

    if (ev->type() == QEvent::FocusIn && ob == d->editor())
        d->focus();

    return QObject::eventFilter(ob, ev);
}

void FakeVimHandler::installEventFilter()
{
    d->installEventFilter();
}

void FakeVimHandler::setupWidget()
{
    d->setupWidget();
}

void FakeVimHandler::restoreWidget(int tabSize)
{
    d->restoreWidget(tabSize);
}

void FakeVimHandler::handleCommand(const QString &cmd)
{
    d->enterFakeVim();
    d->handleCommand(cmd);
    d->leaveFakeVim();
}

void FakeVimHandler::handleReplay(const QString &keys)
{
    d->enterFakeVim();
    d->replay(keys);
    d->leaveFakeVim();
}

void FakeVimHandler::handleInput(const QString &keys)
{
    Inputs inputs(keys);
    d->enterFakeVim();
    foreach (const Input &input, inputs)
        d->handleKey(input);
    d->leaveFakeVim();
}

void FakeVimHandler::enterCommandMode()
{
    d->enterCommandMode();
}

void FakeVimHandler::setCurrentFileName(const QString &fileName)
{
    d->m_currentFileName = fileName;
}

QString FakeVimHandler::currentFileName() const
{
    return d->m_currentFileName;
}

void FakeVimHandler::showMessage(MessageLevel level, const QString &msg)
{
    d->showMessage(level, msg);
}

QWidget *FakeVimHandler::widget()
{
    return d->editor();
}

// Test only
int FakeVimHandler::physicalIndentation(const QString &line) const
{
    Column ind = d->indentation(line);
    return ind.physical;
}

int FakeVimHandler::logicalIndentation(const QString &line) const
{
    Column ind = d->indentation(line);
    return ind.logical;
}

QString FakeVimHandler::tabExpand(int n) const
{
    return d->tabExpand(n);
}

void FakeVimHandler::miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos)
{
    d->miniBufferTextEdited(text, cursorPos, anchorPos);
}

void FakeVimHandler::setTextCursorPosition(int position)
{
    int pos = qMax(0, qMin(position, d->lastPositionInDocument()));
    if (d->isVisualMode())
        d->setPosition(pos);
    else
        d->setAnchorAndPosition(pos, pos);
    d->setTargetColumn();

    if (!d->m_inFakeVim)
        d->commitCursor();
}

QTextCursor FakeVimHandler::textCursor() const
{
    return d->m_cursor;
}

void FakeVimHandler::setTextCursor(const QTextCursor &cursor)
{
    d->m_cursor = cursor;
}

bool FakeVimHandler::jumpToLocalMark(QChar mark, bool backTickMode)
{
    return d->jumpToMark(mark, backTickMode);
}

} // namespace Internal
} // namespace FakeVim

Q_DECLARE_METATYPE(FakeVim::Internal::FakeVimHandler::Private::BufferDataPtr)

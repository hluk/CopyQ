/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QDebug>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRegExp>
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

#include <algorithm>
#include <climits>
#include <ctype.h>

//#define DEBUG_KEY  1
#if DEBUG_KEY
#   define KEY_DEBUG(s) qDebug() << s
#else
#   define KEY_DEBUG(s)
#endif

//#define DEBUG_UNDO  1
#if DEBUG_UNDO
#   define UNDO_DEBUG(s) qDebug() << "REV" << revision() << s
#else
#   define UNDO_DEBUG(s)
#endif

using namespace Utils;
#ifdef FAKEVIM_STANDALONE
using namespace FakeVim::Internal::Utils;
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

#define MetaModifier     // Use HostOsInfo::controlModifier() instead
#define ControlModifier  // Use HostOsInfo::controlModifier() instead

typedef QLatin1String _;

/* Clipboard MIME types used by Vim. */
static const QString vimMimeText = _("_VIM_TEXT");
static const QString vimMimeTextEncoded = _("_VIMENC_TEXT");

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
    CtrlVSubMode         // Used for Ctrl-v in insert mode
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
    \value RangeLineModeExclusice Like \l RangeLineMode, but keeps one
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
    CursorPosition() : line(-1), column(-1) {}
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

    int line; // Line in document (from 0, folded lines included).
    int column; // Position on line.
};

QDebug operator<<(QDebug ts, const CursorPosition &pos)
{
    return ts << "(line: " << pos.line << ", column: " << pos.column << ")";
}

class Mark
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

private:
    CursorPosition m_position;
    QString m_fileName;
};
typedef QHash<QChar, Mark> Marks;
typedef QHashIterator<QChar, Mark> MarksIterator;

struct State
{
    State() : revision(-1), position(), marks(), lastVisualMode(NoVisualMode),
        lastVisualModeInverted(false) {}
    State(int revision, const CursorPosition &position, const Marks &marks,
        VisualMode lastVisualMode, bool lastVisualModeInverted) : revision(revision),
        position(position), marks(marks), lastVisualMode(lastVisualMode),
        lastVisualModeInverted(lastVisualModeInverted) {}

    bool isValid() const { return position.isValid(); }

    int revision;
    CursorPosition position;
    Marks marks;
    VisualMode lastVisualMode;
    bool lastVisualModeInverted;
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
    Register() : rangemode(RangeCharMode) {}
    Register(const QString &c) : contents(c), rangemode(RangeCharMode) {}
    Register(const QString &c, RangeMode m) : contents(c), rangemode(m) {}
    QString contents;
    RangeMode rangemode;
};

QDebug operator<<(QDebug ts, const Register &reg)
{
    return ts << reg.contents;
}

struct SearchData
{
    SearchData()
    {
        forward = true;
        highlightMatches = true;
    }

    QString needle;
    bool forward;
    bool highlightMatches;
};

// If string begins with given prefix remove it with trailing spaces and return true.
static bool eatString(const char *prefix, QString *str)
{
    if (!str->startsWith(_(prefix)))
        return false;
    *str = str->mid(strlen(prefix)).trimmed();
    return true;
}

static QRegExp vimPatternToQtPattern(QString needle, bool ignoreCaseOption, bool smartCaseOption)
{
    /* Transformations (Vim regexp -> QRegExp):
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
        && !(smartCaseOption && needle.contains(QRegExp(_("[A-Z]"))));
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
            if (c == QLatin1Char(']')) {
                pattern.append(_("\\[\\]"));
                continue;
            }
            pattern.append(QLatin1Char('['));
            escape = true;
            embraced = true;
        }
        if (embraced) {
            if (range) {
                QChar c2 = pattern[pattern.size() - 2];
                pattern.remove(pattern.size() - 2, 2);
                pattern.append(c2.toUpper() + QLatin1Char('-') + c.toUpper());
                pattern.append(c2.toLower() + QLatin1Char('-') + c.toLower());
                range = false;
            } else if (escape) {
                escape = false;
                pattern.append(c);
            } else if (c == QLatin1Char('\\')) {
                escape = true;
            } else if (c == QLatin1Char(']')) {
                pattern.append(QLatin1Char(']'));
                embraced = false;
            } else if (c == QLatin1Char('-')) {
                range = ignorecase && pattern[pattern.size() - 1].isLetter();
                pattern.append(QLatin1Char('-'));
            } else if (c.isLetter() && ignorecase) {
                pattern.append(c.toLower()).append(c.toUpper());
            } else {
                pattern.append(c);
            }
        } else if (QString::fromLatin1("(){}+|?").indexOf(c) != -1) {
            if (c == QLatin1Char('{')) {
                curly = escape;
            } else if (c == QLatin1Char('}') && curly) {
                curly = false;
                escape = true;
            }

            if (escape)
                escape = false;
            else
                pattern.append(QLatin1Char('\\'));
            pattern.append(c);
        } else if (escape) {
            // escape expression
            escape = false;
            if (c == QLatin1Char('<') || c == QLatin1Char('>'))
                pattern.append(_("\\b"));
            else if (c == QLatin1Char('a'))
                pattern.append(_("[a-zA-Z]"));
            else if (c == QLatin1Char('A'))
                pattern.append(_("[^a-zA-Z]"));
            else if (c == QLatin1Char('h'))
                pattern.append(_("[A-Za-z_]"));
            else if (c == QLatin1Char('H'))
                pattern.append(_("[^A-Za-z_]"));
            else if (c == QLatin1Char('c') || c == QLatin1Char('C'))
                ignorecase = (c == QLatin1Char('c'));
            else if (c == QLatin1Char('l'))
                pattern.append(_("[a-z]"));
            else if (c == QLatin1Char('L'))
                pattern.append(_("[^a-z]"));
            else if (c == QLatin1Char('o'))
                pattern.append(_("[0-7]"));
            else if (c == QLatin1Char('O'))
                pattern.append(_("[^0-7]"));
            else if (c == QLatin1Char('u'))
                pattern.append(_("[A-Z]"));
            else if (c == QLatin1Char('U'))
                pattern.append(_("[^A-Z]"));
            else if (c == QLatin1Char('x'))
                pattern.append(_("[0-9A-Fa-f]"));
            else if (c == QLatin1Char('X'))
                pattern.append(_("[^0-9A-Fa-f]"));
            else if (c == QLatin1Char('='))
                pattern.append(_("?"));
            else
                pattern.append(QLatin1Char('\\') + c);
        } else {
            // unescaped expression
            if (c == QLatin1Char('\\'))
                escape = true;
            else if (c == QLatin1Char('['))
                brace = true;
            else if (c.isLetter() && ignorecase)
                pattern.append(QLatin1Char('[') + c.toLower() + c.toUpper() + QLatin1Char(']'));
            else
                pattern.append(c);
        }
    }
    if (escape)
        pattern.append(QLatin1Char('\\'));
    else if (brace)
        pattern.append(QLatin1Char('['));

    return QRegExp(pattern);
}

static bool afterEndOfLine(const QTextDocument *doc, int position)
{
    return doc->characterAt(position) == ParagraphSeparator
        && doc->findBlock(position).length() > 1;
}

static void searchForward(QTextCursor *tc, QRegExp &needleExp, int *repeat)
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

static void searchBackward(QTextCursor *tc, QRegExp &needleExp, int *repeat)
{
    // Search from beginning of line so that matched text is the same.
    QTextBlock block = tc->block();
    QString line = block.text();

    int i = line.indexOf(needleExp, 0);
    while (i != -1 && i < tc->positionInBlock()) {
        --*repeat;
        i = line.indexOf(needleExp, i + qMax(1, needleExp.matchedLength()));
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
        i = line.indexOf(needleExp, 0);
        while (i != -1) {
            --*repeat;
            i = line.indexOf(needleExp, i + qMax(1, needleExp.matchedLength()));
            if (i == line.size())
                i = -1;
        }
    }

    if (!block.isValid()) {
        *tc = QTextCursor();
        return;
    }

    i = line.indexOf(needleExp, 0);
    while (*repeat < 0) {
        i = line.indexOf(needleExp, i + qMax(1, needleExp.matchedLength()));
        ++*repeat;
    }
    tc->setPosition(block.position() + i);
    tc->setPosition(tc->position() + needleExp.matchedLength(), KeepAnchor);
}

// Commands [[, []
static void bracketSearchBackward(QTextCursor *tc, const QString &needleExp, int repeat)
{
    QRegExp re(needleExp);
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
    QRegExp re(searchWithCommand ? QString(_("^\\}|^\\{")) : needleExp);
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

static bool substituteText(QString *text, QRegExp &pattern, const QString &replacement,
    bool global)
{
    bool substituted = false;
    int pos = 0;
    int right = -1;
    while (true) {
        pos = pattern.indexIn(*text, pos, QRegExp::CaretAtZero);
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
        QString matched = text->mid(pos, pattern.cap(0).size());
        QString repl;
        bool escape = false;
        // insert captured texts
        for (int i = 0; i < replacement.size(); ++i) {
            const QChar &c = replacement[i];
            if (escape) {
                escape = false;
                if (c.isDigit()) {
                    if (c.digitValue() <= pattern.captureCount())
                        repl += pattern.cap(c.digitValue());
                } else {
                    repl += c;
                }
            } else {
                if (c == QLatin1Char('\\'))
                    escape = true;
                else if (c == QLatin1Char('&'))
                    repl += pattern.cap(0);
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
        if (line.at(i) == c && (i == 0 || line.at(i - 1) != QLatin1Char('\\')))
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

    QMimeData *data = new QMimeData;
    data->setText(content);
    data->setData(vimMimeText, bytes1);
    data->setData(vimMimeTextEncoded, bytes2);
    clipboard->setMimeData(data, clipboardMode);
}

static QByteArray toLocalEncoding(const QString &text)
{
    return HostOsInfo::isWindowsHost() ? QString(text).replace(_("\n"), _("\r\n")).toLocal8Bit()
                                       : text.toLocal8Bit();
}

static QString fromLocalEncoding(const QByteArray &data)
{
    return HostOsInfo::isWindowsHost() ? QString::fromLocal8Bit(data).replace(_("\n"), _("\r\n"))
                                       : QString::fromLocal8Bit(data);
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
    static QMap<QString, int> k;
    if (!k.isEmpty())
        return k;

    // FIXME: Should be value of mapleader.
    k.insert(_("LEADER"), Key_Backslash);

    k.insert(_("SPACE"), Key_Space);
    k.insert(_("TAB"), Key_Tab);
    k.insert(_("NL"), Key_Return);
    k.insert(_("NEWLINE"), Key_Return);
    k.insert(_("LINEFEED"), Key_Return);
    k.insert(_("LF"), Key_Return);
    k.insert(_("CR"), Key_Return);
    k.insert(_("RETURN"), Key_Return);
    k.insert(_("ENTER"), Key_Return);
    k.insert(_("BS"), Key_Backspace);
    k.insert(_("BACKSPACE"), Key_Backspace);
    k.insert(_("ESC"), Key_Escape);
    k.insert(_("BAR"), Key_Bar);
    k.insert(_("BSLASH"), Key_Backslash);
    k.insert(_("DEL"), Key_Delete);
    k.insert(_("DELETE"), Key_Delete);
    k.insert(_("KDEL"), Key_Delete);
    k.insert(_("UP"), Key_Up);
    k.insert(_("DOWN"), Key_Down);
    k.insert(_("LEFT"), Key_Left);
    k.insert(_("RIGHT"), Key_Right);

    k.insert(_("LT"), Key_Less);
    k.insert(_("GT"), Key_Greater);

    k.insert(_("F1"), Key_F1);
    k.insert(_("F2"), Key_F2);
    k.insert(_("F3"), Key_F3);
    k.insert(_("F4"), Key_F4);
    k.insert(_("F5"), Key_F5);
    k.insert(_("F6"), Key_F6);
    k.insert(_("F7"), Key_F7);
    k.insert(_("F8"), Key_F8);
    k.insert(_("F9"), Key_F9);
    k.insert(_("F10"), Key_F10);

    k.insert(_("F11"), Key_F11);
    k.insert(_("F12"), Key_F12);
    k.insert(_("F13"), Key_F13);
    k.insert(_("F14"), Key_F14);
    k.insert(_("F15"), Key_F15);
    k.insert(_("F16"), Key_F16);
    k.insert(_("F17"), Key_F17);
    k.insert(_("F18"), Key_F18);
    k.insert(_("F19"), Key_F19);
    k.insert(_("F20"), Key_F20);

    k.insert(_("F21"), Key_F21);
    k.insert(_("F22"), Key_F22);
    k.insert(_("F23"), Key_F23);
    k.insert(_("F24"), Key_F24);
    k.insert(_("F25"), Key_F25);
    k.insert(_("F26"), Key_F26);
    k.insert(_("F27"), Key_F27);
    k.insert(_("F28"), Key_F28);
    k.insert(_("F29"), Key_F29);
    k.insert(_("F30"), Key_F30);

    k.insert(_("F31"), Key_F31);
    k.insert(_("F32"), Key_F32);
    k.insert(_("F33"), Key_F33);
    k.insert(_("F34"), Key_F34);
    k.insert(_("F35"), Key_F35);

    k.insert(_("INSERT"), Key_Insert);
    k.insert(_("INS"), Key_Insert);
    k.insert(_("KINSERT"), Key_Insert);
    k.insert(_("HOME"), Key_Home);
    k.insert(_("END"), Key_End);
    k.insert(_("PAGEUP"), Key_PageUp);
    k.insert(_("PAGEDOWN"), Key_PageDown);

    k.insert(_("KPLUS"), Key_Plus);
    k.insert(_("KMINUS"), Key_Minus);
    k.insert(_("KDIVIDE"), Key_Slash);
    k.insert(_("KMULTIPLY"), Key_Asterisk);
    k.insert(_("KENTER"), Key_Enter);
    k.insert(_("KPOINT"), Key_Period);

    return k;
}

static bool isOnlyControlModifier(const Qt::KeyboardModifiers &mods)
{
    return (mods ^ HostOsInfo::controlModifier()) == Qt::NoModifier;
}


Range::Range()
    : beginPos(-1), endPos(-1), rangemode(RangeCharMode)
{}

Range::Range(int b, int e, RangeMode m)
    : beginPos(qMin(b, e)), endPos(qMax(b, e)), rangemode(m)
{}

QString Range::toString() const
{
    return QString::fromLatin1("%1-%2 (mode: %3)").arg(beginPos).arg(endPos)
        .arg(rangemode);
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
    : cmd(c), hasBang(false), args(a), range(r), count(1)
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
        else if (cc == QLatin1Char('\n'))
            res += _("<CR>");
        else
            res += QString::fromLatin1("\\x%1").arg(c.unicode(), 2, 16, QLatin1Char('0'));
    }
    return res;
}

static bool startsWithWhitespace(const QString &str, int col)
{
    QTC_ASSERT(str.size() >= col, return false);
    for (int i = 0; i < col; ++i) {
        uint u = str.at(i).unicode();
        if (u != QLatin1Char(' ') && u != QLatin1Char('\t'))
            return false;
    }
    return true;
}

inline QString msgMarkNotSet(const QString &text)
{
    return FakeVimHandler::tr("Mark \"%1\" not set.").arg(text);
}

class Input
{
public:
    // Remove some extra "information" on Mac.
    static Qt::KeyboardModifiers cleanModifier(Qt::KeyboardModifiers m)
    {
        return m & ~Qt::KeypadModifier;
    }

    Input()
        : m_key(0), m_xkey(0), m_modifiers(0) {}

    explicit Input(QChar x)
        : m_key(x.unicode()), m_xkey(x.unicode()), m_modifiers(0), m_text(x)
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
            if (x.unicode() < ' ')
                m_text.clear();
            else if (x.isLetter())
                m_key = x.toUpper().unicode();
        }

        // Set text only if input is ascii key without control modifier.
        if (m_text.isEmpty() && k >= 0 && k <= 0x7f && (m & HostOsInfo::controlModifier()) == 0) {
            QChar c = QChar::fromLatin1(k);
            if (c.isLetter())
                m_text = QString(isShift() ? c.toUpper() : c);
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
        return m_key == QLatin1Char('\n') || m_key == Key_Return || m_key == Key_Enter;
    }

    bool isEscape() const
    {
        return isKey(Key_Escape) || isKey(27) || isControl('c')
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
        if (!m_text.isEmpty() && !a.m_text.isEmpty() && m_text != _(" "))
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
            return QLatin1Char('\t');
        if (m_key == Key_Return)
            return QLatin1Char('\n');
        if (m_key == Key_Escape)
            return QChar(27);
        return m_xkey;
    }

    QString toString() const
    {
        QString key = vimKeyNames().key(m_key);
        bool namedKey = !key.isEmpty();

        if (!namedKey) {
            if (m_xkey == '<')
                key = _("<LT>");
            else if (m_xkey == '>')
                key = _("<GT>");
            else
                key = QChar(m_xkey);
        }

        bool shift = isShift();
        bool ctrl = isControl();
        if (shift)
            key.prepend(_("S-"));
        if (ctrl)
            key.prepend(_("C-"));

        if (namedKey || shift || ctrl) {
            key.prepend(QLatin1Char('<'));
            key.append(QLatin1Char('>'));
        }

        return key;
    }

    QDebug dump(QDebug ts) const
    {
        return ts << m_key << '-' << m_modifiers << '-'
            << quoteUnprintable(m_text);
    }
private:
    int m_key;
    int m_xkey;
    Qt::KeyboardModifiers m_modifiers;
    QString m_text;
};

// mapping to <Nop> (do nothing)
static const Input Nop(-1, Qt::KeyboardModifiers(-1), QString());

QDebug operator<<(QDebug ts, const Input &input) { return input.dump(ts); }

class Inputs : public QVector<Input>
{
public:
    Inputs() : m_noremap(true), m_silent(false) {}

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

    bool m_noremap;
    bool m_silent;
};

static Input parseVimKeyName(const QString &keyName)
{
    if (keyName.length() == 1)
        return Input(keyName.at(0));

    const QStringList keys = keyName.split(QLatin1Char('-'));
    const int len = keys.length();

    if (len == 1 && keys.at(0).toUpper() == _("NOP"))
        return Nop;

    Qt::KeyboardModifiers mods = NoModifier;
    for (int i = 0; i < len - 1; ++i) {
        const QString &key = keys[i].toUpper();
        if (key == _("S"))
            mods |= Qt::ShiftModifier;
        else if (key == _("C"))
            mods |= HostOsInfo::controlModifier();
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
        ushort c = str.at(i).unicode();
        if (c == '<') {
            int j = str.indexOf(QLatin1Char('>'), i);
            Input input;
            if (j != -1) {
                const QString key = str.mid(i+1, j - i - 1);
                if (!key.contains(QLatin1Char('<')))
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

class History
{
public:
    History() : m_items(QString()), m_index(0) {}
    void append(const QString &item);
    const QString &move(const QStringRef &prefix, int skip);
    const QString &current() const { return m_items[m_index]; }
    const QStringList &items() const { return m_items; }
    void restart() { m_index = m_items.size() - 1; }

private:
    // Last item is always empty or current search prefix.
    QStringList m_items;
    int m_index;
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
class CommandBuffer
{
public:
    CommandBuffer() : m_pos(0), m_anchor(0), m_userPos(0), m_historyAutoSave(true) {}

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
                msg += QLatin1Char('^');
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
    int m_pos;
    int m_anchor;
    int m_userPos; // last position of inserted text (for retrieving history items)
    bool m_historyAutoSave; // store items to history on clear()?
};

// Mappings for a specific mode (trie structure)
class ModeMapping : public QMap<Input, ModeMapping>
{
public:
    const Inputs &value() const { return m_value; }
    void setValue(const Inputs &value) { m_value = value; }
private:
    Inputs m_value;
};

// Mappings for all modes
typedef QHash<char, ModeMapping> Mappings;

// Iterator for mappings
class MappingsIterator : public QVector<ModeMapping::Iterator>
{
public:
    MappingsIterator(Mappings *mappings, char mode = -1, const Inputs &inputs = Inputs())
        : m_parent(mappings)
        , m_lastValid(-1)
        , m_mode(0)
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
    int m_lastValid;
    char m_mode;
    Inputs m_currentInputs;
};

// state of current mapping
struct MappingState {
    MappingState()
        : noremap(false), silent(false), editBlock(false) {}
    MappingState(bool noremap, bool silent, bool editBlock)
        : noremap(noremap), silent(silent), editBlock(editBlock) {}
    bool noremap;
    bool silent;
    bool editBlock;
};

class FakeVimHandler::Private : public QObject
{
    Q_OBJECT

public:
    Private(FakeVimHandler *parent, QWidget *widget);

    EventResult handleEvent(QKeyEvent *ev);
    bool wantsOverride(QKeyEvent *ev);
    bool parseExCommmand(QString *line, ExCommand *cmd);
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

    // Call before any FakeVim processing (import cursor position from editor)
    void enterFakeVim();
    // Call after any FakeVim processing
    // (if needUpdate is true, export cursor position to editor and scroll)
    void leaveFakeVim(bool needUpdate = true);

    EventResult handleKey(const Input &input);
    EventResult handleDefaultKey(const Input &input);
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
    bool handleChangeDeleteSubModes(const Input &);
    bool handleReplaceSubMode(const Input &);
    bool handleFilterSubMode(const Input &);
    bool handleRegisterSubMode(const Input &);
    bool handleShiftSubMode(const Input &);
    bool handleChangeCaseSubMode(const Input &);
    bool handleWindowSubMode(const Input &);
    bool handleYankSubMode(const Input &);
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
    void resetCommandMode();
    void clearCommandMode();
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

    bool atEmptyLine(const QTextCursor &tc = QTextCursor()) const;
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
    void movePageDown(int count = 1);
    void movePageUp(int count = 1) { movePageDown(-count); }
    void dump(const char *msg) const {
        qDebug() << msg << "POS: " << anchor() << position()
            << "EXT: " << m_oldExternalAnchor << m_oldExternalPosition
            << "INT: " << m_oldInternalAnchor << m_oldInternalPosition
            << "VISUAL: " << g.visualMode;
    }
    void moveRight(int n = 1) {
        //dump("RIGHT 1");
        if (isVisualCharMode()) {
            const QTextBlock currentBlock = block();
            const int max = currentBlock.position() + currentBlock.length() - 1;
            const int pos = position() + n;
            setPosition(qMin(pos, max));
        } else {
            m_cursor.movePosition(Right, KeepAnchor, n);
        }
        if (atEndOfLine())
            emit q->fold(1, false);
        //dump("RIGHT 2");
    }
    void moveLeft(int n = 1) {
        m_cursor.movePosition(Left, KeepAnchor, n);
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
    void commitCursor() {
        if (isVisualBlockMode()) {
            emit q->requestSetBlockSelection(m_cursor);
        } else  {
            emit q->requestDisableBlockSelection();
            if (editor())
                EDITOR(setTextCursor(m_cursor));
        }
    }
    // Restore cursor from editor widget.
    void pullCursor() {
        if (isVisualBlockMode())
            q->requestBlockSelection(&m_cursor);
        else if (editor())
            m_cursor = EDITOR(textCursor());
    }

    // Values to save when starting FakeVim processing.
    int m_firstVisibleLine;
    QTextCursor m_cursor;

    bool moveToPreviousParagraph(int count) { return moveToNextParagraph(-count); }
    bool moveToNextParagraph(int count);

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
    QWidget *editor() const;
    QTextDocument *document() const { return EDITOR(document()); }
    QChar characterAtCursor() const
        { return document()->characterAt(position()); }

    void joinPreviousEditBlock();
    void beginEditBlock(bool largeEditBlock = false);
    void beginLargeEditBlock() { beginEditBlock(true); }
    void endEditBlock();
    void breakEditBlock() { m_buffer->breakEditBlock = true; }

    Q_SLOT void onContentsChanged(int position, int charsRemoved, int charsAdded);
    Q_SLOT void onUndoCommandAdded();

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
    bool selectBlockTextObject(bool inner, char left, char right);
    bool selectQuotedStringTextObject(bool inner, const QString &quote);

    Q_SLOT void importSelection();
    void exportSelection();
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
    bool executeRegister(int register);

public:
    QTextEdit *m_textedit;
    QPlainTextEdit *m_plaintextedit;
    bool m_wasReadOnly; // saves read-only state of document

    bool m_inFakeVim; // true if currently processing a key press or a command

    FakeVimHandler *q;
    int m_oldExternalPosition; // copy from last event to check for external changes
    int m_oldExternalAnchor;
    int m_oldInternalPosition; // copy from last event to check for external changes
    int m_oldInternalAnchor;
    int m_register;
    BlockInsertMode m_visualBlockInsert;

    bool m_fakeEnd;
    bool m_anchorPastEnd;
    bool m_positionPastEnd; // '$' & 'l' in visual mode can move past eol

    QString m_currentFileName;

    int m_findStartPosition;

    int anchor() const { return m_cursor.anchor(); }
    int position() const { return m_cursor.position(); }

    struct TransformationData
    {
        TransformationData(const QString &s, const QVariant &d)
            : from(s), extraData(d) {}
        QString from;
        QString to;
        QVariant extraData;
    };
    typedef void (Private::*Transformation)(TransformationData *td);
    void transformText(const Range &range, Transformation transformation,
        const QVariant &extraData = QVariant());

    void insertText(QTextCursor &tc, const QString &text);
    void insertText(const Register &reg);
    void removeText(const Range &range);
    void removeTransform(TransformationData *td);

    void invertCase(const Range &range);
    void invertCaseTransform(TransformationData *td);

    void upCase(const Range &range);
    void upCaseTransform(TransformationData *td);

    void downCase(const Range &range);
    void downCaseTransform(TransformationData *td);

    void replaceText(const Range &range, const QString &str);
    void replaceByStringTransform(TransformationData *td);
    void replaceByCharTransform(TransformationData *td);

    QString selectText(const Range &range) const;
    void setCurrentRange(const Range &range);
    Range currentRange() const { return Range(position(), anchor(), g.rangemode); }

    void yankText(const Range &range, int toregister);

    void pasteText(bool afterCursor);

    void joinLines(int count, bool preserveSpace = false);

    void insertNewLine();

    bool handleInsertInEditor(const Input &input);
    bool passEventToEditor(QEvent &event); // Pass event to editor widget without filtering. Returns true if event was processed.

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

    // marks
    Mark mark(QChar code) const;
    void setMark(QChar code, CursorPosition position);
    // jump to valid mark return true if mark is valid and local
    bool jumpToMark(QChar mark, bool backTickMode);
    // update marks on undo/redo
    void updateMarks(const Marks &newMarks);
    CursorPosition markLessPosition() const { return mark(QLatin1Char('<')).position(document()); }
    CursorPosition markGreaterPosition() const { return mark(QLatin1Char('>')).position(document()); }

    // vi style configuration
    QVariant config(int code) const { return theFakeVimSetting(code)->value(); }
    bool hasConfig(int code) const { return config(code).toBool(); }
    bool hasConfig(int code, const char *value) const // FIXME
        { return config(code).toString().contains(_(value)); }

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
    void getRegisterType(int reg, bool *isClipboard, bool *isSelection) const;

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
    bool handleExShiftCommand(const ExCommand &cmd);
    bool handleExSourceCommand(const ExCommand &cmd);
    bool handleExSubstituteCommand(const ExCommand &cmd);
    bool handleExWriteCommand(const ExCommand &cmd);
    bool handleExEchoCommand(const ExCommand &cmd);

    void timerEvent(QTimerEvent *ev);

    void setupCharClass();
    int charClass(QChar c, bool simple) const;
    signed char m_charClass[256];

    int m_ctrlVAccumulator;
    int m_ctrlVLength;
    int m_ctrlVBase;

    void miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos);

    // Data shared among editors with same document.
    struct BufferData
    {
        BufferData()
            : lastRevision(0)
            , editBlockLevel(0)
            , breakEditBlock(false)
            , lastVisualMode(NoVisualMode)
            , lastVisualModeInverted(false)
        {}

        QStack<State> undo;
        QStack<State> redo;
        State undoState;
        int lastRevision;

        int editBlockLevel; // current level of edit blocks
        bool breakEditBlock; // if true, joinPreviousEditBlock() starts new edit block

        QStack<CursorPosition> jumpListUndo;
        QStack<CursorPosition> jumpListRedo;
        CursorPosition lastChangePosition;

        VisualMode lastVisualMode;
        bool lastVisualModeInverted;

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
    };

    typedef QSharedPointer<BufferData> BufferDataPtr;
    void pullOrCreateBufferData();
    BufferDataPtr m_buffer;

    // Data shared among all editors.
    static struct GlobalData
    {
        GlobalData()
            : passing(false)
            , mode(CommandMode)
            , submode(NoSubMode)
            , subsubmode(NoSubSubMode)
            , visualMode(NoVisualMode)
            , mvcount(0)
            , opcount(0)
            , movetype(MoveInclusive)
            , rangemode(RangeCharMode)
            , gflag(false)
            , mappings()
            , currentMap(&mappings)
            , inputTimer(-1)
            , mapDepth(0)
            , currentMessageLevel(MessageInfo)
            , lastSearchForward(false)
            , highlightsCleared(false)
            , findPending(false)
            , returnToMode(CommandMode)
            , currentRegister(0)
            , lastExecutedRegister(0)
        {
            commandBuffer.setPrompt(QLatin1Char(':'));
        }

        // Current state.
        bool passing; // let the core see the next event
        Mode mode;
        SubMode submode;
        SubSubMode subsubmode;
        Input subsubdata;
        VisualMode visualMode;

        // [count] for current command, 0 if no [count] available
        int mvcount;
        int opcount;

        MoveType movetype;
        RangeMode rangemode;
        bool gflag;  // whether current command started with 'g'

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
        int inputTimer;
        QStack<MappingState> mapStates;
        int mapDepth;

        // Command line buffers.
        CommandBuffer commandBuffer;
        CommandBuffer searchBuffer;

        // Current mini buffer message.
        QString currentMessage;
        MessageLevel currentMessageLevel;
        QString currentCommand;

        // Search state.
        QString lastSearch; // last search expression as entered by user
        QString lastNeedle; // last search expression translated with vimPatternToQtPattern()
        bool lastSearchForward; // last search command was '/' or '*'
        bool highlightsCleared; // ':nohlsearch' command is active until next search
        bool findPending; // currently searching using external tool (until editor is focused again)

        // Last substitution command.
        QString lastSubstituteFlags;
        QString lastSubstitutePattern;
        QString lastSubstituteReplacement;

        // Global marks.
        Marks marks;

        // Return to insert/replace mode after single command (<C-O>).
        Mode returnToMode;

        // Currently recorded macro (not recording if null string).
        QString recording;
        int currentRegister;
        int lastExecutedRegister;
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
        connect(EDITOR(document()), SIGNAL(contentsChange(int,int,int)),
                SLOT(onContentsChanged(int,int,int)));
        connect(EDITOR(document()), SIGNAL(undoCommandAdded()), SLOT(onUndoCommandAdded()));
        m_buffer->lastRevision = revision();
    }
}

void FakeVimHandler::Private::init()
{
    m_inFakeVim = false;
    m_findStartPosition = -1;
    m_visualBlockInsert = NoneBlockInsertMode;
    m_fakeEnd = false;
    m_positionPastEnd = false;
    m_anchorPastEnd = false;
    m_register = '"';
    m_targetColumn = 0;
    m_visualTargetColumn = 0;
    m_targetColumnWrapped = 0;
    m_oldInternalAnchor = -1;
    m_oldInternalPosition = -1;
    m_oldExternalAnchor = -1;
    m_oldExternalPosition = -1;
    m_searchStartPosition = 0;
    m_searchFromScreenLine = 0;
    m_firstVisibleLine = 0;
    m_ctrlVAccumulator = 0;
    m_ctrlVLength = 0;
    m_ctrlVBase = 0;

    pullOrCreateBufferData();
    setupCharClass();
}

void FakeVimHandler::Private::focus()
{
    enterFakeVim();

    stopIncrementalFind();
    if (!isInsertMode()) {
        if (g.subsubmode == SearchSubSubMode) {
            setPosition(m_searchStartPosition);
            scrollToLine(m_searchFromScreenLine);
            setTargetColumn();
            setAnchor();
            commitCursor();
        } else if (g.submode != NoSubMode || g.mode == ExMode) {
            leaveVisualMode();
            setPosition(qMin(position(), anchor()));
            setTargetColumn();
            setAnchor();
            commitCursor();
        }

        bool exitCommandLine = (g.subsubmode == SearchSubSubMode || g.mode == ExMode);
        resetCommandMode();
        if (exitCommandLine)
            updateMiniBuffer();
    }
    updateCursorShape();
    if (g.mode != CommandMode)
        updateMiniBuffer();
    updateHighlights();

    leaveFakeVim(false);
}

void FakeVimHandler::Private::enterFakeVim()
{
    QTC_ASSERT(!m_inFakeVim, qDebug() << "enterFakeVim() shouldn't be called recursively!"; return);

    pullOrCreateBufferData();

    pullCursor();
    if (m_cursor.isNull())
        m_cursor = QTextCursor(document());

    m_inFakeVim = true;

    removeEventFilter();

    updateFirstVisibleLine();
    importSelection();

    // Position changed externally, e.g. by code completion.
    if (position() != m_oldInternalPosition) {
        // record external jump to different line
        if (m_oldInternalPosition != -1 && lineForPosition(m_oldInternalPosition) != lineForPosition(position()))
            recordJump(m_oldInternalPosition);
        setTargetColumn();
        if (atEndOfLine() && !isVisualMode() && !isInsertMode())
            moveLeft();
    }

    if (m_fakeEnd)
        moveRight();
}

void FakeVimHandler::Private::leaveFakeVim(bool needUpdate)
{
    QTC_ASSERT(m_inFakeVim, qDebug() << "enterFakeVim() not called before leaveFakeVim()!"; return);

    // The command might have destroyed the editor.
    if (m_textedit || m_plaintextedit) {
        // We fake vi-style end-of-line behaviour
        m_fakeEnd = atEndOfLine() && g.mode == CommandMode && !isVisualBlockMode()
            && !isVisualCharMode();

        //QTC_ASSERT(g.mode == InsertMode || g.mode == ReplaceMode
        //        || !atBlockEnd() || block().length() <= 1,
        //    qDebug() << "Cursor at EOL after key handler");
        if (m_fakeEnd)
            moveLeft();

        if (hasConfig(ConfigShowMarks))
            updateSelection();

        exportSelection();
        updateCursorShape();

        if (needUpdate) {
            commitCursor();

            // Move cursor line to middle of screen if it's not visible.
            const int line = cursorLine();
            if (line < firstVisibleLine() || line > firstVisibleLine() + linesOnScreen())
                scrollToLine(qMax(0, line - linesOnScreen() / 2));
            else
                scrollToLine(firstVisibleLine());
            updateScrollOffset();
        }

        installEventFilter();
    }

    m_inFakeVim = false;
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
        //updateMiniBuffer();
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
        g.passing = false;
        updateMiniBuffer();
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
    //emit q->requestHasBlockSelection(&hasBlock);
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

    enterFakeVim();
    EventResult result = handleKey(Input(key, mods, ev->text()));
    leaveFakeVim(result == EventHandled);

    return result;
}

void FakeVimHandler::Private::installEventFilter()
{
    EDITOR(viewport()->installEventFilter(q));
    EDITOR(installEventFilter(q));
}

void FakeVimHandler::Private::removeEventFilter()
{
    EDITOR(viewport()->removeEventFilter(q));
    EDITOR(removeEventFilter(q));
}

void FakeVimHandler::Private::setupWidget()
{
    enterFakeVim();

    resetCommandMode();
    m_wasReadOnly = EDITOR(isReadOnly());

    updateEditor();
    importSelection();
    updateMiniBuffer();
    updateCursorShape();

    recordJump();
    setTargetColumn();
    if (atEndOfLine() && !isVisualMode() && !isInsertMode())
        moveLeft();

    leaveFakeVim();
}

void FakeVimHandler::Private::exportSelection()
{
    int pos = position();
    int anc = isVisualMode() ? anchor() : position();

    m_oldInternalPosition = pos;
    m_oldInternalAnchor = anc;

    if (isVisualMode()) {
        if (g.visualMode == VisualBlockMode) {
            const int col1 = anc - document()->findBlock(anc).position();
            const int col2 = pos - document()->findBlock(pos).position();
            if (col1 > col2)
                ++anc;
            else if (!atBlockEnd())
                ++pos;
            // FIXME: After '$' command (i.e. m_visualTargetColumn == -1), end of selected lines
            //        should be selected.
            setAnchorAndPosition(anc, pos);
            commitCursor();
        } else if (g.visualMode == VisualLineMode) {
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
            if (!document()->findBlock(pos).isVisible())
                ++pos;
            setAnchorAndPosition(anc, pos);
        } else if (g.visualMode == VisualCharMode) {
            if (anc > pos)
                ++anc;
        } else {
            QTC_CHECK(false);
        }

        setAnchorAndPosition(anc, pos);

        setMark(QLatin1Char('<'), markLessPosition());
        setMark(QLatin1Char('>'), markGreaterPosition());
    } else {
        if (g.subsubmode == SearchSubSubMode && !m_searchCursor.isNull())
            m_cursor = m_searchCursor;
        else
            setAnchorAndPosition(pos, pos);
    }
    m_oldExternalPosition = position();
    m_oldExternalAnchor = anchor();
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
        const ushort c = document()->characterAt(pos).unicode();
        if (c == '<')
            lastInsertion.replace(i, 1, _("<LT>"));
        else if ((c == ' ' || c == '\t') && insertState.spaces.contains(pos))
            lastInsertion.replace(i, 1, _(c == ' ' ? "<SPACE>" : "<TAB>"));
    }

    // Remove unnecessary backspaces.
    while (insertState.backspaces > 0 && !lastInsertion.isEmpty() && lastInsertion[0].isSpace())
        --insertState.backspaces;

    // backspaces in front of inserted text
    lastInsertion.prepend(QString(_("<BS>")).repeated(insertState.backspaces));
    // deletes after inserted text
    lastInsertion.prepend(QString(_("<DELETE>")).repeated(insertState.deletes));

    // Remove indentation.
    lastInsertion.replace(QRegExp(_("(^|\n)[\\t ]+")), _("\\1"));
}

void FakeVimHandler::Private::invalidateInsertState()
{
    m_oldInternalPosition = position();
    BufferData::InsertState &insertState = m_buffer->insertState;
    insertState.pos1 = -1;
    insertState.pos2 = m_oldInternalPosition;
    insertState.backspaces = 0;
    insertState.deletes = 0;
    insertState.spaces.clear();
    insertState.insertingSpaces = false;
    insertState.textBeforeCursor = textAt(document()->findBlock(m_oldInternalPosition).position(),
                                            m_oldInternalPosition);
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
    QTextBlock block = document()->findBlock(start);
    QTextBlock block2 = document()->findBlock(end);
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

void FakeVimHandler::Private::importSelection()
{
    if (position() == m_oldExternalPosition
            && anchor() == m_oldExternalAnchor) {
        // Undo drawing correction.
        setAnchorAndPosition(m_oldInternalAnchor, m_oldInternalPosition);
    } else {
        // Import new selection.
        Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
        if (m_cursor.hasSelection()) {
            if (mods & HostOsInfo::controlModifier())
                g.visualMode = VisualBlockMode;
            else if (mods & Qt::AltModifier)
                g.visualMode = VisualBlockMode;
            else if (mods & Qt::ShiftModifier)
                g.visualMode = VisualLineMode;
            else
                g.visualMode = VisualCharMode;
            m_buffer->lastVisualMode = g.visualMode;
        } else {
            g.visualMode = NoVisualMode;
        }
    }
}

void FakeVimHandler::Private::updateEditor()
{
    const int charWidth = QFontMetrics(EDITOR(font())).width(QLatin1Char(' '));
    EDITOR(setTabStopWidth(charWidth * config(ConfigTabStop).toInt()));
    setupCharClass();
}

void FakeVimHandler::Private::restoreWidget(int tabSize)
{
    //clearMessage();
    //updateMiniBuffer();
    //EDITOR(removeEventFilter(q));
    //EDITOR(setReadOnly(m_wasReadOnly));
    const int charWidth = QFontMetrics(EDITOR(font())).width(QLatin1Char(' '));
    EDITOR(setTabStopWidth(charWidth * tabSize));
    g.visualMode = NoVisualMode;
    // Force "ordinary" cursor.
    EDITOR(setOverwriteMode(false));
    updateSelection();
    updateHighlights();
}

EventResult FakeVimHandler::Private::handleKey(const Input &input)
{
    KEY_DEBUG("HANDLE INPUT: " << input << " MODE: " << mode);

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

EventResult FakeVimHandler::Private::handleDefaultKey(const Input &input)
{
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
        showMessage(MessageError, tr("Recursive mapping"));
        updateMiniBuffer();
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
    updateMiniBuffer();
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
    updateMiniBuffer();

    // wait for user to press any key or trigger complete mapping after interval
    g.inputTimer = startTimer(1000);
}

EventResult FakeVimHandler::Private::stopWaitForMapping(bool hasInput)
{
    if (g.inputTimer != -1) {
        killTimer(g.inputTimer);
        g.inputTimer = -1;
        g.currentCommand.clear();
        if (!hasInput && !expandCompleteMapping()) {
            // Cannot complete mapping so handle the first input from it as default command.
            return handleCurrentMapAsDefault();
        }
    }

    return EventHandled;
}

void FakeVimHandler::Private::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == g.inputTimer) {
        enterFakeVim();
        EventResult result = handleKey(Input());
        leaveFakeVim(result == EventHandled);
    }
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

bool FakeVimHandler::Private::atEmptyLine(const QTextCursor &tc) const
{
    if (tc.isNull())
        return atEmptyLine(m_cursor);
    return tc.block().length() == 1;
}

bool FakeVimHandler::Private::atBoundary(bool end, bool simple, bool onlyWords,
    const QTextCursor &tc) const
{
    if (tc.isNull())
        return atBoundary(end, simple, onlyWords, m_cursor);
    if (atEmptyLine(tc))
        return true;
    int pos = tc.position();
    QChar c1 = document()->characterAt(pos);
    QChar c2 = document()->characterAt(pos + (end ? 1 : -1));
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
    for (int i = document()->findBlock(pos).position(); i < pos; ++i) {
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

    m_buffer->redo.clear();
    m_buffer->lastChangePosition = CursorPosition(document(), pos);
    if (isVisualMode()) {
        setMark(QLatin1Char('<'), markLessPosition());
        setMark(QLatin1Char('>'), markGreaterPosition());
    }
    m_buffer->undoState = State(revision(), m_buffer->lastChangePosition, m_buffer->marks,
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

bool FakeVimHandler::Private::moveToNextParagraph(int count)
{
    const bool forward = count > 0;
    int repeat = forward ? count : -count;
    int pos = position();
    QTextBlock block = this->block();

    if (block.isValid() && block.length() == 1)
        ++repeat;

    for (; block.isValid(); block = forward ? block.next() : block.previous()) {
        if (block.length() == 1) {
            if (--repeat == 0)
                break;
            while (block.isValid() && block.length() == 1)
                block = forward ? block.next() : block.previous();
        }
    }

    if (repeat == 0)
        setPosition(block.position());
    else if (repeat == 1)
        setPosition(forward ? lastPositionInDocument() : 0);
    else
        return false;

    recordJump(pos);
    setTargetColumn();
    g.movetype = MoveExclusive;

    return true;
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
    emit q->fold(1, false);
    int pos = qMin(block().position() + block().length() - 1,
        lastPositionInDocument() + 1);
    setPosition(pos);
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
        if (document()->characterAt(position()) == ParagraphSeparator) {
            if (!atEmptyLine()) {
                setPosition(position() + 1);
                return;
            }
        } else if (document()->characterAt(anchor()) == ParagraphSeparator) {
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
        enterExMode(QString::fromLatin1(".,+%1!").arg(qAbs(endLine - beginLine)));
        return;
    }

    if (g.submode == ChangeSubMode
        || g.submode == DeleteSubMode
        || g.submode == YankSubMode
        || g.submode == InvertCaseSubMode
        || g.submode == DownCaseSubMode
        || g.submode == UpCaseSubMode) {
        fixSelection();

        if (g.submode != InvertCaseSubMode
            && g.submode != DownCaseSubMode
            && g.submode != UpCaseSubMode) {
            yankText(currentRange(), m_register);
            if (g.movetype == MoveLineWise)
                setRegister(m_register, registerContents(m_register), RangeLineMode);
        }

        m_positionPastEnd = m_anchorPastEnd = false;
    }

    QString dotCommand;
    if (g.submode == ChangeSubMode) {
        pushUndoState(false);
        beginEditBlock();
        removeText(currentRange());
        dotCommand = _("c");
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
        dotCommand = _("d");
        if (g.movetype == MoveLineWise)
            handleStartOfLine();
        if (atEndOfLine())
            moveLeft();
        else
            setTargetColumn();
        endEditBlock();
    } else if (g.submode == YankSubMode) {
        bool isVisualModeYank = isVisualMode();
        leaveVisualMode();
        const QTextCursor tc = m_cursor;
        if (g.rangemode == RangeBlockMode) {
            const int pos1 = tc.block().position();
            const int pos2 = document()->findBlock(tc.anchor()).position();
            const int col = qMin(tc.position() - pos1, tc.anchor() - pos2);
            setPosition(qMin(pos1, pos2) + col);
        } else {
            setPosition(qMin(position(), anchor()));
            if (g.rangemode == RangeLineMode) {
                if (isVisualModeYank)
                    moveToStartOfLine();
            }
        }
        setTargetColumn();
    } else if (g.submode == InvertCaseSubMode
        || g.submode == UpCaseSubMode
        || g.submode == DownCaseSubMode) {
        beginEditBlock();
        if (g.submode == InvertCaseSubMode) {
            invertCase(currentRange());
            dotCommand = QString::fromLatin1("g~");
        } else if (g.submode == DownCaseSubMode) {
            downCase(currentRange());
            dotCommand = QString::fromLatin1("gu");
        } else if (g.submode == UpCaseSubMode) {
            upCase(currentRange());
            dotCommand = QString::fromLatin1("gU");
        }
        if (g.movetype == MoveLineWise)
            handleStartOfLine();
        endEditBlock();
    } else if (g.submode == IndentSubMode
        || g.submode == ShiftRightSubMode
        || g.submode == ShiftLeftSubMode) {
        recordJump();
        pushUndoState(false);
        if (g.submode == IndentSubMode) {
            indentSelectedText();
            dotCommand = _("=");
        } else if (g.submode == ShiftRightSubMode) {
            shiftRegionRight(1);
            dotCommand = _(">");
        } else if (g.submode == ShiftLeftSubMode) {
            shiftRegionLeft(1);
            dotCommand = _("<");
        }
    }

    if (!dotCommand.isEmpty() && !dotCommandMovement.isEmpty())
        setDotCommand(dotCommand + dotCommandMovement);

    // Change command continues in insert mode.
    if (g.submode == ChangeSubMode) {
        clearCommandMode();
        enterInsertMode();
    } else {
        resetCommandMode();
    }
}

void FakeVimHandler::Private::resetCommandMode()
{
    if (g.returnToMode == CommandMode) {
        enterCommandMode();
    } else {
        clearCommandMode();
        const QString lastInsertion = m_buffer->lastInsertion;
        if (g.returnToMode == InsertMode)
            enterInsertMode();
        else
            enterReplaceMode();
        moveToTargetColumn();
        invalidateInsertState();
        m_buffer->lastInsertion = lastInsertion;
    }
    if (isNoVisualMode())
        setAnchor();
}

void FakeVimHandler::Private::clearCommandMode()
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
        for (MarksIterator it(m_buffer->marks); it.hasNext(); ) {
            it.next();
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
    emit q->selectionChanged(selections);
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

    emit q->highlightMatches(m_highlighted);
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
        msg = _("PASSING");
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
            msg = _("-- VISUAL --");
        else if (isVisualLineMode())
            msg = _("-- VISUAL LINE --");
        else if (isVisualBlockMode())
            msg = _("VISUAL BLOCK");
    } else if (g.mode == InsertMode) {
        msg = _("-- INSERT --");
    } else if (g.mode == ReplaceMode) {
        msg = _("-- REPLACE --");
    } else {
        QTC_CHECK(g.mode == CommandMode && g.subsubmode != SearchSubSubMode);
        if (g.returnToMode == CommandMode)
            msg = _("-- COMMAND --");
        else if (g.returnToMode == InsertMode)
            msg = _("-- (insert) --");
        else
            msg = _("-- (replace) --");
    }

    if (!g.recording.isNull() && msg.startsWith(_("--")))
        msg.append(_("recording"));

    emit q->commandBufferChanged(msg, cursorPos, anchorPos, messageLevel, q);

    int linesInDoc = linesInDocument();
    int l = cursorLine();
    QString status;
    const QString pos = QString::fromLatin1("%1,%2")
        .arg(l + 1).arg(physicalCursorColumn() + 1);
    // FIXME: physical "-" logical
    if (linesInDoc != 0)
        status = FakeVimHandler::tr("%1%2%").arg(pos, -10).arg(l * 100 / linesInDoc, 4);
    else
        status = FakeVimHandler::tr("%1All").arg(pos, -10);
    emit q->statusDataChanged(status);
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
    showMessage(MessageError, FakeVimHandler::tr("Not implemented in FakeVim."));
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
    //const int key = input.key;
    bool handled = true;
    if (g.subsubmode == FtSubSubMode) {
        g.semicolonType = g.subsubdata;
        g.semicolonKey = input.text();
        bool valid = handleFfTt(g.semicolonKey);
        g.subsubmode = NoSubSubMode;
        if (!valid) {
            g.submode = NoSubMode;
            resetCommandMode();
            handled = false;
        } else {
            finishMovement(QString::fromLatin1("%1%2%3")
                           .arg(count())
                           .arg(g.semicolonType.text())
                           .arg(g.semicolonKey));
        }
    } else if (g.subsubmode == TextObjectSubSubMode) {
        bool ok = true;
        if (input.is('w'))
            selectWordTextObject(g.subsubdata.is('i'));
        else if (input.is('W'))
            selectWORDTextObject(g.subsubdata.is('i'));
        else if (input.is('s'))
            selectSentenceTextObject(g.subsubdata.is('i'));
        else if (input.is('p'))
            selectParagraphTextObject(g.subsubdata.is('i'));
        else if (input.is('[') || input.is(']'))
            ok = selectBlockTextObject(g.subsubdata.is('i'), '[', ']');
        else if (input.is('(') || input.is(')') || input.is('b'))
            ok = selectBlockTextObject(g.subsubdata.is('i'), '(', ')');
        else if (input.is('<') || input.is('>'))
            ok = selectBlockTextObject(g.subsubdata.is('i'), '<', '>');
        else if (input.is('{') || input.is('}') || input.is('B'))
            ok = selectBlockTextObject(g.subsubdata.is('i'), '{', '}');
        else if (input.is('"') || input.is('\'') || input.is('`'))
            ok = selectQuotedStringTextObject(g.subsubdata.is('i'), input.asChar());
        else
            ok = false;
        g.subsubmode = NoSubSubMode;
        if (ok) {
            finishMovement(QString::fromLatin1("%1%2%3")
                           .arg(count())
                           .arg(g.subsubdata.text())
                           .arg(input.text()));
        } else {
            resetCommandMode();
            handled = false;
        }
    } else if (g.subsubmode == MarkSubSubMode) {
        setMark(input.asChar(), CursorPosition(m_cursor));
        g.subsubmode = NoSubSubMode;
    } else if (g.subsubmode == BackTickSubSubMode
            || g.subsubmode == TickSubSubMode) {
        if (jumpToMark(input.asChar(), g.subsubmode == BackTickSubSubMode)) {
            finishMovement();
        } else {
            resetCommandMode();
            handled = false;
        }
        g.subsubmode = NoSubSubMode;
    } else if (g.subsubmode == ZSubSubMode) {
        handled = false;
        if (input.is('j') || input.is('k')) {
            int pos = position();
            emit q->foldGoTo(input.is('j') ? count() : -count(), false);
            if (pos != position()) {
                handled = true;
                finishMovement(QString::fromLatin1("%1z%2")
                               .arg(count())
                               .arg(input.text()));
            }
        }
    } else if (g.subsubmode == OpenSquareSubSubMode || g.subsubmode == CloseSquareSubSubMode) {
        int pos = position();
        if (input.is('{') && g.subsubmode == OpenSquareSubSubMode)
            searchBalanced(false, QLatin1Char('{'), QLatin1Char('}'));
        else if (input.is('}') && g.subsubmode == CloseSquareSubSubMode)
            searchBalanced(true, QLatin1Char('}'), QLatin1Char('{'));
        else if (input.is('(') && g.subsubmode == OpenSquareSubSubMode)
            searchBalanced(false, QLatin1Char('('), QLatin1Char(')'));
        else if (input.is(')') && g.subsubmode == CloseSquareSubSubMode)
            searchBalanced(true, QLatin1Char(')'), QLatin1Char('('));
        else if (input.is('[') && g.subsubmode == OpenSquareSubSubMode)
            bracketSearchBackward(&m_cursor, _("^\\{"), count());
        else if (input.is('[') && g.subsubmode == CloseSquareSubSubMode)
            bracketSearchForward(&m_cursor, _("^\\}"), count(), false);
        else if (input.is(']') && g.subsubmode == OpenSquareSubSubMode)
            bracketSearchBackward(&m_cursor, _("^\\}"), count());
        else if (input.is(']') && g.subsubmode == CloseSquareSubSubMode)
            bracketSearchForward(&m_cursor, _("^\\{"), count(), g.submode != NoSubMode);
        else if (input.is('z'))
            emit q->foldGoTo(g.subsubmode == OpenSquareSubSubMode ? -count() : count(), true);
        handled = pos != position();
        if (handled) {
            if (lineForPosition(pos) != lineForPosition(position()))
                recordJump(pos);
            finishMovement(QString::fromLatin1("%1%2%3")
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
    QString movement;
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
            emit q->findRequested(!g.lastSearchForward);
        } else {
            // FIXME: make core find dialog sufficiently flexible to
            // produce the "default vi" behaviour too. For now, roll our own.
            g.currentMessage.clear();
            g.movetype = MoveExclusive;
            g.subsubmode = SearchSubSubMode;
            g.searchBuffer.setPrompt(g.lastSearchForward ? QLatin1Char('/') : QLatin1Char('?'));
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
        needle = QRegExp::escape(tc.selection().toPlainText());
        if (!g.gflag)
            needle = _("\\<") + needle + _("\\>");
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
        moveRight(qMin(count, rightDist()) - 1);
        setTargetColumn();
    } else if (input.is('}')) {
        handled = moveToNextParagraph(count);
    } else if (input.is('{')) {
        handled = moveToPreviousParagraph(count);
    } else if (input.isReturn()) {
        moveToStartOfLine();
        moveDown();
        moveToFirstNonBlankOnLine();
        g.movetype = MoveLineWise;
    } else if (input.is('-')) {
        moveToStartOfLine();
        moveUp(count);
        moveToFirstNonBlankOnLine();
        g.movetype = MoveLineWise;
    } else if (input.is('+')) {
        moveToStartOfLine();
        moveDown(count);
        moveToFirstNonBlankOnLine();
        g.movetype = MoveLineWise;
    } else if (input.isKey(Key_Home)) {
        moveToStartOfLine();
        setTargetColumn();
        movement = _("<HOME>");
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
        setTargetColumn();
        if (g.submode == NoSubMode)
            m_targetColumn = -1;
        if (isVisualMode())
            m_visualTargetColumn = -1;
        movement = _("$");
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
        g.movetype = MoveExclusive;
        moveToNextWordStart(count, false, false);
        setTargetColumn();
        movement = _("b");
    } else if (input.is('B')) {
        g.movetype = MoveExclusive;
        moveToNextWordStart(count, true, false);
        setTargetColumn();
    } else if (input.is('e') && g.gflag) {
        g.movetype = MoveInclusive;
        moveToNextWordEnd(count, false, false);
        setTargetColumn();
    } else if (input.is('e') || input.isShift(Key_Right)) {
        g.movetype = MoveInclusive;
        moveToNextWordEnd(count, false, true, false);
        setTargetColumn();
        movement = _("e");
    } else if (input.is('E') && g.gflag) {
        g.movetype = MoveInclusive;
        moveToNextWordEnd(count, true, false);
        setTargetColumn();
    } else if (input.is('E')) {
        g.movetype = MoveInclusive;
        moveToNextWordEnd(count, true, true, false);
        setTargetColumn();
    } else if (input.isControl('e')) {
        // FIXME: this should use the "scroll" option, and "count"
        if (cursorLineOnScreen() == 0)
            moveDown(1);
        scrollDown(1);
        movement = _("<C-E>");
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
        QString dotCommand = QString::fromLatin1("%1G").arg(count);
        recordJump();
        if (input.is('G') && g.mvcount == 0)
            dotCommand = QString(QLatin1Char('G'));
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
        if (m_fakeEnd && block().length() > 1)
            ++n;
        moveLeft(n);
        setTargetColumn();
        movement = _("h");
    } else if (input.is('H')) {
        const CursorPosition pos(lineToBlockNumber(lineOnTop(count)), 0);
        setCursorPosition(&m_cursor, pos);
        handleStartOfLine();
    } else if (input.is('j') || input.isKey(Key_Down)
            || input.isControl('j') || input.isControl('n')) {
        if (g.gflag) {
            g.movetype = MoveExclusive;
            moveDownVisually(count);
            movement = _("gj");
        } else {
            g.movetype = MoveLineWise;
            moveDown(count);
            movement = _("j");
        }
    } else if (input.is('k') || input.isKey(Key_Up) || input.isControl('p')) {
        if (g.gflag) {
            g.movetype = MoveExclusive;
            moveUpVisually(count);
            movement = _("gk");
        } else {
            g.movetype = MoveLineWise;
            moveUp(count);
            movement = _("k");
        }
    } else if (input.is('l') || input.isKey(Key_Right) || input.is(' ')) {
        g.movetype = MoveExclusive;
        bool pastEnd = count >= rightDist() - 1;
        moveRight(qMax(0, qMin(count, rightDist() - (g.submode == NoSubMode))));
        setTargetColumn();
        if (pastEnd && isVisualMode())
            m_visualTargetColumn = -1;
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
            emit q->findNextRequested(!forward);
            if (forward && pos == m_cursor.selectionStart()) {
                // if cursor is already positioned at the start of a find result, this is returned
                emit q->findNextRequested(false);
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
    } else if (input.is('w') || input.is('W')) { // tested
        // Special case: "cw" and "cW" work the same as "ce" and "cE" if the
        // cursor is on a non-blank - except if the cursor is on the last
        // character of a word: only the current word will be changed
        bool simple = input.is('W');
        if (g.submode == ChangeSubMode && !document()->characterAt(position()).isSpace()) {
            moveToWordEnd(count, simple, true);
            g.movetype = MoveInclusive;
        } else {
            moveToNextWordStart(count, simple, true);
            // Command 'dw' deletes to the next word on the same line or to end of line.
            if (g.submode == DeleteSubMode && count == 1) {
                const QTextBlock currentBlock = document()->findBlock(anchor());
                setPosition(qMin(position(), currentBlock.position() + currentBlock.length()));
            }
            g.movetype = MoveExclusive;
        }
        setTargetColumn();
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
        movement = _("f");
    } else if (input.isKey(Key_PageUp) || input.isControl('b')) {
        movePageUp(count);
        handleStartOfLine();
        movement = _("b");
    } else {
        handled = false;
    }

    if (handled && g.subsubmode == NoSubSubMode) {
        if (g.submode == NoSubMode) {
            resetCommandMode();
        } else {
            // finish movement for sub modes
            const QString dotMovement =
                (count > 1 ? QString::number(count) : QString())
                + _(g.gflag ? "g" : "")
                + (movement.isNull() ? QString(input.asChar()) : movement);
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
    } else if (g.subsubmode != NoSubSubMode) {
        handled = handleCommandSubSubMode(input);
    } else if (g.submode == NoSubMode) {
        handled = handleNoSubMode(input);
    } else if (g.submode == ChangeSubMode || g.submode == DeleteSubMode) {
        handled = handleChangeDeleteSubModes(input);
    } else if (g.submode == ReplaceSubMode) {
        handled = handleReplaceSubMode(input);
    } else if (g.submode == FilterSubMode) {
        handled = handleFilterSubMode(input);
    } else if (g.submode == RegisterSubMode) {
        handled = handleRegisterSubMode(input);
    } else if (g.submode == WindowSubMode) {
        handled = handleWindowSubMode(input);
    } else if (g.submode == YankSubMode) {
        handled = handleYankSubMode(input);
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
            resetCommandMode();
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
    } else {
        resetCommandMode();
        //qDebug() << "IGNORED IN COMMAND MODE: " << key << text
        //    << " VISUAL: " << g.visualMode;

        // if a key which produces text was pressed, don't mark it as unhandled
        // - otherwise the text would be inserted while being in command mode
        if (input.text().isEmpty())
            handled = false;
    }

    updateMiniBuffer();

    m_positionPastEnd = (m_visualTargetColumn == -1) && isVisualMode() && !atEmptyLine();

    return handled ? EventHandled : EventCancelled;
}

bool FakeVimHandler::Private::handleEscape()
{
    if (isVisualMode())
        leaveVisualMode();
    resetCommandMode();
    return true;
}

bool FakeVimHandler::Private::handleNoSubMode(const Input &input)
{
    bool handled = true;

    if (input.is('&')) {
        handleExCommand(g.gflag ? _("%s//~/&") : _("s"));
    } else if (input.is(':')) {
        enterExMode();
    } else if (input.is('!') && isNoVisualMode()) {
        g.submode = FilterSubMode;
    } else if (input.is('!') && isVisualMode()) {
        enterExMode(QString::fromLatin1("!"));
    } else if (input.is('"')) {
        g.submode = RegisterSubMode;
    } else if (input.is(',')) {
        passShortcuts(true);
    } else if (input.is('.')) {
        //qDebug() << "REPEATING" << quoteUnprintable(g.dotCommand) << count()
        //    << input;
        QString savedCommand = g.dotCommand;
        g.dotCommand.clear();
        beginLargeEditBlock();
        replay(savedCommand);
        endEditBlock();
        resetCommandMode();
        g.dotCommand = savedCommand;
    } else if (input.is('<') || input.is('>') || input.is('=')) {
        if (isNoVisualMode()) {
            if (input.is('<'))
                g.submode = ShiftLeftSubMode;
            else if (input.is('>'))
                g.submode = ShiftRightSubMode;
            else
                g.submode = IndentSubMode;
            setAnchor();
        } else {
            leaveVisualMode();
            const int lines = qAbs(lineForPosition(position()) - lineForPosition(anchor())) + 1;
            const int repeat = count();
            if (input.is('<'))
                shiftRegionLeft(repeat);
            else if (input.is('>'))
                shiftRegionRight(repeat);
            else
                indentSelectedText();
            const QString selectDotCommand =
                    (lines > 1) ? QString::fromLatin1("V%1j").arg(lines - 1): QString();
            setDotCommand(selectDotCommand + QString::fromLatin1("%1%2%2").arg(repeat).arg(input.raw()));
        }
    } else if ((!isVisualMode() && input.is('a')) || (isVisualMode() && input.is('A'))) {
        if (isVisualMode()) {
            enterVisualInsertMode(QLatin1Char('A'));
        } else {
            setDotCommand(_("%1a"), count());
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
        setDotCommand(_("%1A"), count());
    } else if (input.isControl('a')) {
        if (changeNumberTextObject(count()))
            setDotCommand(_("%1<c-a>"), count());
    } else if ((input.is('c') || input.is('d')) && isNoVisualMode()) {
        setAnchor();
        g.opcount = g.mvcount;
        g.mvcount = 0;
        g.rangemode = RangeCharMode;
        g.movetype = MoveExclusive;
        g.submode = input.is('c') ? ChangeSubMode : DeleteSubMode;
    } else if ((input.is('c') || input.is('C') || input.is('s') || input.is('R'))
          && (isVisualCharMode() || isVisualLineMode())) {
        setDotCommand(visualDotCommand() + input.asChar());
        leaveVisualMode();
        g.submode = ChangeSubMode;
        finishMovement();
    } else if ((input.is('c') || input.is('s')) && isVisualBlockMode()) {
        resetCount();
        enterVisualInsertMode(input.asChar());
    } else if (input.is('C')) {
        setAnchor();
        moveToEndOfLine();
        g.rangemode = RangeCharMode;
        g.submode = ChangeSubMode;
        setDotCommand(QString(QLatin1Char('C')));
        finishMovement();
    } else if (input.isControl('c')) {
        if (isNoVisualMode())
            showMessage(MessageInfo, tr("Type Alt-V, Alt-V to quit FakeVim mode."));
        else
            leaveVisualMode();
    } else if ((input.is('d') || input.is('x') || input.isKey(Key_Delete))
            && isVisualMode()) {
        pushUndoState();
        setDotCommand(visualDotCommand() + QLatin1Char('x'));
        if (isVisualCharMode()) {
            leaveVisualMode();
            g.submode = DeleteSubMode;
            finishMovement();
        } else if (isVisualLineMode()) {
            leaveVisualMode();
            yankText(currentRange(), m_register);
            removeText(currentRange());
            handleStartOfLine();
        } else if (isVisualBlockMode()) {
            leaveVisualMode();
            yankText(currentRange(), m_register);
            removeText(currentRange());
            setPosition(qMin(position(), anchor()));
        }
    } else if (input.is('D') && isNoVisualMode()) {
        pushUndoState();
        if (atEndOfLine())
            moveLeft();
        g.submode = DeleteSubMode;
        g.movetype = MoveInclusive;
        setAnchorAndPosition(position(), lastPositionInLine(cursorLine() + count()));
        setDotCommand(QString(QLatin1Char('D')));
        finishMovement();
        setTargetColumn();
    } else if ((input.is('D') || input.is('X')) &&
         (isVisualCharMode() || isVisualLineMode())) {
        setDotCommand(visualDotCommand() + QLatin1Char('X'));
        leaveVisualMode();
        g.rangemode = RangeLineMode;
        g.submode = NoSubMode;
        yankText(currentRange(), m_register);
        removeText(currentRange());
        moveToFirstNonBlankOnLine();
    } else if ((input.is('D') || input.is('X')) && isVisualBlockMode()) {
        setDotCommand(visualDotCommand() + QLatin1Char('X'));
        leaveVisualMode();
        g.rangemode = RangeBlockAndTailMode;
        yankText(currentRange(), m_register);
        removeText(currentRange());
        setPosition(qMin(position(), anchor()));
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
        setDotCommand(_("%1i"), count());
        breakEditBlock();
        enterInsertMode();
        if (atEndOfLine())
            moveLeft();
    } else if (input.is('I')) {
        if (isVisualMode()) {
            enterVisualInsertMode(QLatin1Char('I'));
        } else {
            if (g.gflag) {
                setDotCommand(_("%1gI"), count());
                moveToStartOfLine();
            } else {
                setDotCommand(_("%1I"), count());
                moveToFirstNonBlankOnLine();
            }
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
        setDotCommand(_("%1J"), count());
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
        setDotCommand(_(insertAfter ? "%1o" : "%1O"), count());
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
            m_oldInternalPosition = position();
            m_buffer->insertState.pos1 = m_oldInternalPosition;
            m_buffer->insertState.newLineAfter = true;
        }
        setTargetColumn();
        endEditBlock();

        // Close accidentally opened block.
        if (block().blockNumber() > 0) {
            moveUp();
            if (line != lineNumber(block()))
                emit q->fold(1, true);
            moveDown();
        }
    } else if (input.isControl('o')) {
        jump(-count());
    } else if (input.is('p') || input.is('P') || input.isShift(Qt::Key_Insert)) {
        pasteText(!input.is('P'));
        setTargetColumn();
        setDotCommand(_("%1p"), count());
        finishMovement();
    } else if (input.is('q')) {
        if (g.recording.isNull()) {
            // Recording shouldn't work in mapping or while executing register.
            handled = g.mapStates.empty();
            if (handled)
                g.submode = MacroRecordSubMode;
        } else {
            // Stop recording.
            stopRecording();
        }
    } else if (input.is('r')) {
        g.submode = ReplaceSubMode;
    } else if (!isVisualMode() && input.is('R')) {
        pushUndoState();
        breakEditBlock();
        enterReplaceMode();
    } else if (input.isControl('r')) {
        int repeat = count();
        while (--repeat >= 0)
            redo();
    } else if (input.is('s')) {
        pushUndoState();
        leaveVisualMode();
        if (atEndOfLine())
            moveLeft();
        setAnchor();
        moveRight(qMin(count(), rightDist()));
        setDotCommand(_("%1s"), count());
        g.submode = ChangeSubMode;
        g.movetype = MoveExclusive;
        finishMovement();
    } else if (input.is('S')) {
        g.movetype = MoveLineWise;
        pushUndoState();
        if (!isVisualMode()) {
            const int line = cursorLine() + 1;
            const int anc = firstPositionInLine(line);
            const int pos = lastPositionInLine(line + count() - 1);
            setAnchorAndPosition(anc, pos);
        }
        setDotCommand(_("%1S"), count());
        g.submode = ChangeSubMode;
        finishMovement();
    } else if (g.gflag && input.is('t')) {
        handleExCommand(_("tabnext"));
    } else if (g.gflag && input.is('T')) {
        handleExCommand(_("tabprev"));
    } else if (input.isControl('t')) {
        handleExCommand(_("pop"));
    } else if (!g.gflag && input.is('u') && !isVisualMode()) {
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
        if (m_buffer->lastVisualMode != NoVisualMode) {
            CursorPosition from = markLessPosition();
            CursorPosition to = markGreaterPosition();
            toggleVisualMode(m_buffer->lastVisualMode);
            setCursorPosition(m_buffer->lastVisualModeInverted ? to : from);
            setAnchor();
            setCursorPosition(m_buffer->lastVisualModeInverted ? from : to);
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
    } else if (input.is('x') && isNoVisualMode()) { // = _("dl")
        g.movetype = MoveExclusive;
        g.submode = DeleteSubMode;
        const int n = qMin(count(), rightDist());
        setAnchorAndPosition(position(), position() + n);
        setDotCommand(_("%1x"), count());
        finishMovement();
    } else if (input.isControl('x')) {
        if (changeNumberTextObject(-count()))
            setDotCommand(_("%1<c-x>"), count());
    } else if (input.is('X')) {
        if (leftDist() > 0) {
            setAnchor();
            moveLeft(qMin(count(), leftDist()));
            yankText(currentRange(), m_register);
            removeText(currentRange());
        }
    } else if (input.is('Y') && isNoVisualMode())  {
        handleYankSubMode(Input(QLatin1Char('y')));
    } else if (input.isControl('y')) {
        // FIXME: this should use the "scroll" option, and "count"
        if (cursorLineOnScreen() == linesOnScreen() - 1)
            moveUp(1);
        scrollUp(1);
    } else if (input.is('y') && isNoVisualMode()) {
        setAnchor();
        g.rangemode = RangeCharMode;
        g.movetype = MoveExclusive;
        g.submode = YankSubMode;
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
        pushUndoState();
        if (isVisualMode()) {
            setDotCommand(visualDotCommand() + QString::number(count()) + input.raw());
            if (isVisualLineMode())
                g.rangemode = RangeLineMode;
            else if (isVisualBlockMode())
                g.rangemode = RangeBlockMode;
            leaveVisualMode();
            if (input.is('~'))
                g.submode = InvertCaseSubMode;
            else if (input.is('u'))
                g.submode = DownCaseSubMode;
            else if (input.is('U'))
                g.submode = UpCaseSubMode;
            finishMovement();
        } else if (g.gflag || (input.is('~') && hasConfig(ConfigTildeOp))) {
            if (atEndOfLine())
                moveLeft();
            setAnchor();
            if (input.is('~'))
                g.submode = InvertCaseSubMode;
            else if (input.is('u'))
                g.submode = DownCaseSubMode;
            else if (input.is('U'))
                g.submode = UpCaseSubMode;
        } else {
            beginEditBlock();
            if (atEndOfLine())
                moveLeft();
            setAnchor();
            moveRight(qMin(count(), rightDist()));
            if (input.is('~')) {
                const int pos = position();
                invertCase(currentRange());
                setPosition(pos);
            } else if (input.is('u')) {
                downCase(currentRange());
            } else if (input.is('U')) {
                upCase(currentRange());
            }
            setDotCommand(QString::fromLatin1("%1%2").arg(count()).arg(input.raw()));
            endEditBlock();
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
        handleExCommand(_("tag"));
    } else if (handleMovement(input)) {
        // movement handled
    } else {
        handled = false;
    }

    return handled;
}

bool FakeVimHandler::Private::handleChangeDeleteSubModes(const Input &input)
{
    bool handled = false;

    if ((g.submode == ChangeSubMode && input.is('c'))
        || (g.submode == DeleteSubMode && input.is('d'))) {
        g.movetype = MoveLineWise;
        pushUndoState();
        const int anc = firstPositionInLine(cursorLine() + 1);
        moveDown(count() - 1);
        const int pos = lastPositionInLine(cursorLine() + 1);
        setAnchorAndPosition(anc, pos);
        if (g.submode == ChangeSubMode)
            setDotCommand(_("%1cc"), count());
        else
            setDotCommand(_("%1dd"), count());
        finishMovement();
        g.submode = NoSubMode;
        handled = true;
    }

    return handled;
}

bool FakeVimHandler::Private::handleReplaceSubMode(const Input &input)
{
    bool handled = true;

    setDotCommand(visualDotCommand() + QLatin1Char('r') + input.asChar());
    if (isVisualMode()) {
        pushUndoState();
        if (isVisualLineMode())
            g.rangemode = RangeLineMode;
        else if (isVisualBlockMode())
            g.rangemode = RangeBlockMode;
        else
            g.rangemode = RangeCharMode;
        leaveVisualMode();
        Range range = currentRange();
        if (g.rangemode == RangeCharMode)
            ++range.endPos;
        Transformation tr =
                &FakeVimHandler::Private::replaceByCharTransform;
        transformText(range, tr, input.asChar());
    } else if (count() <= rightDist()) {
        pushUndoState();
        setAnchor();
        moveRight(count());
        Range range = currentRange();
        if (input.isReturn()) {
            beginEditBlock();
            replaceText(range, QString());
            insertText(QString::fromLatin1("\n"));
            endEditBlock();
        } else {
            replaceText(range, QString(count(), input.asChar()));
            moveRight(count() - 1);
        }
        setTargetColumn();
        setDotCommand(_("%1r") + input.text(), count());
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
    if (QString::fromLatin1("*+.%#:-\"").contains(reg) || reg.isLetterOrNumber()) {
        m_register = reg.unicode();
        g.rangemode = RangeLineMode;
        handled = true;
    }
    g.submode = NoSubMode;

    return handled;
}

bool FakeVimHandler::Private::handleShiftSubMode(const Input &input)
{
    bool handled = false;
    if ((g.submode == ShiftLeftSubMode && input.is('<'))
        || (g.submode == ShiftRightSubMode && input.is('>'))
        || (g.submode == IndentSubMode && input.is('='))) {
        g.movetype = MoveLineWise;
        pushUndoState();
        moveDown(count() - 1);
        setDotCommand(QString::fromLatin1("%2%1%1").arg(input.asChar()), count());
        finishMovement();
        handled = true;
        g.submode = NoSubMode;
    }
    return handled;
}

bool FakeVimHandler::Private::handleChangeCaseSubMode(const Input &input)
{
    bool handled = false;
    if ((g.submode == InvertCaseSubMode && input.is('~'))
        || (g.submode == DownCaseSubMode && input.is('u'))
        || (g.submode == UpCaseSubMode && input.is('U'))) {
        if (!isFirstNonBlankOnLine(position())) {
            moveToStartOfLine();
            moveToFirstNonBlankOnLine();
        }
        setTargetColumn();
        pushUndoState();
        setAnchor();
        setPosition(lastPositionInLine(cursorLine() + count()) + 1);
        finishMovement(QString::fromLatin1("%1%2").arg(count()).arg(input.raw()));
        handled = true;
        g.submode = NoSubMode;
    }
    return handled;
}

bool FakeVimHandler::Private::handleWindowSubMode(const Input &input)
{
    if (handleCount(input))
        return true;

    leaveVisualMode();
    emit q->windowCommandRequested(input.toString(), count());

    g.submode = NoSubMode;
    return true;
}

bool FakeVimHandler::Private::handleYankSubMode(const Input &input)
{
    bool handled = false;
    if (input.is('y')) {
        g.movetype = MoveLineWise;
        int endPos = firstPositionInLine(lineForPosition(position()) + count() - 1);
        Range range(position(), endPos, RangeLineMode);
        yankText(range, m_register);
        g.submode = NoSubMode;
        handled = true;
    }
    return handled;
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
        emit q->fold(count(), foldMaybeClosed);
    } else if (input.is('O') || input.is('C')) {
        // Recursively open/close current fold.
        foldMaybeClosed = input.is('C');
        emit q->fold(-1, foldMaybeClosed);
    } else if (input.is('a') || input.is('A')) {
        // Toggle current fold.
        foldMaybeClosed = true;
        emit q->foldToggle(input.is('a') ? count() : -1);
    } else if (input.is('R') || input.is('M')) {
        // Open/close all folds in document.
        foldMaybeClosed = input.is('M');
        emit q->foldAll(foldMaybeClosed);
    } else if (input.is('j') || input.is('k')) {
        emit q->foldGoTo(input.is('j') ? count() : -count(), false);
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
        handleExCommand(QString(QLatin1Char('x')));
    else if (input.is('Q'))
        handleExCommand(_("q!"));
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
    } else if (m_oldInternalPosition == position()) {
        setTargetColumn();
    }

    updateMiniBuffer();

    // We don't want fancy stuff in insert mode.
    return EventHandled;
}

void FakeVimHandler::Private::handleReplaceMode(const Input &input)
{
    if (input.isEscape()) {
        commitInsertState();
        moveLeft(qMin(1, leftDist()));
        enterCommandMode();
        g.dotCommand.append(m_buffer->lastInsertion + _("<ESC>"));
    } else if (input.isKey(Key_Left)) {
        moveLeft();
        setTargetColumn();
    } else if (input.isKey(Key_Right)) {
        moveRight();
        setTargetColumn();
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
            text.prepend(_("<END>\n"));
        } else if (newLineBefore) {
            text.prepend(_("<END>"));
        }

        replay(text, repeat);

        if (m_visualBlockInsert != NoneBlockInsertMode && !text.contains(QLatin1Char('\n'))) {
            const CursorPosition lastAnchor = markLessPosition();
            const CursorPosition lastPosition = markGreaterPosition();
            bool change = m_visualBlockInsert == ChangeBlockInsertMode;
            const int insertColumn = (m_visualBlockInsert == InsertBlockInsertMode || change)
                    ? qMin(lastPosition.column, lastAnchor.column)
                    : qMax(lastPosition.column, lastAnchor.column) + 1;

            CursorPosition pos(lastAnchor.line, insertColumn);

            if (change)
                pos.column = m_buffer->insertState.pos1 - document()->findBlock(m_buffer->insertState.pos1).position();

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
                        m_cursor.insertText(QString(_(" ")).repeated(spaces));
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
        m_buffer->lastInsertion.remove(0, m_buffer->lastInsertion.indexOf(QLatin1Char('\n')) + 1);
    g.dotCommand.append(m_buffer->lastInsertion + _("<ESC>"));

    enterCommandMode();
    setTargetColumn();
}

void FakeVimHandler::Private::handleInsertMode(const Input &input)
{
    if (input.isEscape()) {
        finishInsertMode();
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
    } else if (input.isControl('w')) {
        const int blockNumber = m_cursor.blockNumber();
        const int endPos = position();
        moveToNextWordStart(1, false, false);
        if (blockNumber != m_cursor.blockNumber())
            moveToEndOfLine();
        const int beginPos = position();
        Range range(beginPos, endPos, RangeCharMode);
        removeText(range);
    } else if (input.isKey(Key_Insert)) {
        g.mode = ReplaceMode;
    } else if (input.isKey(Key_Left)) {
        moveLeft();
        setTargetColumn();
    } else if (input.isControl(Key_Left)) {
        moveToNextWordStart(1, false, false);
        setTargetColumn();
    } else if (input.isKey(Key_Down)) {
        g.submode = NoSubMode;
        moveDown();
    } else if (input.isKey(Key_Up)) {
        g.submode = NoSubMode;
        moveUp();
    } else if (input.isKey(Key_Right)) {
        moveRight();
        setTargetColumn();
    } else if (input.isControl(Key_Right)) {
        moveToNextWordStart(1, false, true);
        moveRight(); // we need one more move since we are in insert mode
        setTargetColumn();
    } else if (input.isKey(Key_Home)) {
        moveToStartOfLine();
        setTargetColumn();
    } else if (input.isKey(Key_End)) {
        moveBehindEndOfLine();
        setTargetColumn();
        m_targetColumn = -1;
    } else if (input.isReturn() || input.isControl('j') || input.isControl('m')) {
        if (!input.isReturn() || !handleInsertInEditor(input)) {
            joinPreviousEditBlock();
            g.submode = NoSubMode;
            insertNewLine();
            endEditBlock();
        }
    } else if (input.isBackspace()) {
        if (!handleInsertInEditor(input)) {
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
            QString str = QString(ts - col % ts, QLatin1Char(' '));
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
            if (text.at(i) == QLatin1Char(' '))
                ++amount;
            else if (text.at(i) == QLatin1Char('\t'))
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
        emit q->simpleCompletionRequested(str, input.isControl('n'));
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
    if (reg == QLatin1Char('"') || reg.isLetterOrNumber()) {
        g.currentRegister = reg.unicode();
        g.recording = QLatin1String("");
        return true;
    }

    return false;
}

void FakeVimHandler::Private::record(const Input &input)
{
    if ( !g.recording.isNull() )
        g.recording.append(input.toString());
}

void FakeVimHandler::Private::stopRecording()
{
    // Remove q from end (stop recording command).
    g.recording.remove(g.recording.size() - 1, 1);
    setRegister(g.currentRegister, g.recording, g.rangemode);
    g.currentRegister = 0;
    g.recording = QString();
}

bool FakeVimHandler::Private::executeRegister(int reg)
{
    QChar regChar(reg);

    // TODO: Prompt for an expression to execute if register is '='.
    if (reg == '@' && g.lastExecutedRegister != 0)
        reg = g.lastExecutedRegister;
    else if (QString::fromLatin1("\".*+").contains(regChar) || regChar.isLetterOrNumber())
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
    if (input.isEscape()) {
        g.commandBuffer.clear();
        resetCommandMode();
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
            resetCommandMode();
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
        if (m_textedit || m_plaintextedit)
            leaveVisualMode();
    } else if (!g.commandBuffer.handleInput(input)) {
        qDebug() << "IGNORED IN EX-MODE: " << input.key() << input.text();
        return EventUnhandled;
    }
    updateMiniBuffer();
    return EventHandled;
}

EventResult FakeVimHandler::Private::handleSearchSubSubMode(const Input &input)
{
    EventResult handled = EventHandled;

    if (input.isEscape()) {
        g.currentMessage.clear();
        setPosition(m_searchStartPosition);
        scrollToLine(m_searchFromScreenLine);
    } else if (input.isBackspace()) {
        if (g.searchBuffer.isEmpty())
            resetCommandMode();
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
                finishMovement(g.searchBuffer.prompt() + g.lastSearch + QLatin1Char('\n'));
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
        resetCommandMode();
        updateMiniBuffer();
    } else {
        updateMiniBuffer();
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
    if (c == QLatin1Char('.')) { // current line
        result = cursorBlockNumber();
        cmd->remove(0, 1);
    } else if (c == QLatin1Char('$')) { // last line
        result = document()->blockCount() - 1;
        cmd->remove(0, 1);
    } else if (c == QLatin1Char('\'')) { // mark
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
    } else if (c == QLatin1Char('-') || c == QLatin1Char('+')) { // add or subtract from current line number
        result = cursorBlockNumber();
    } else if (c == QLatin1Char('/') || c == QLatin1Char('?')
        || (c == QLatin1Char('\\') && cmd->size() > 1 && QString::fromLatin1("/?&").contains(cmd->at(1)))) {
        // search for expression
        SearchData sd;
        if (c == QLatin1Char('/') || c == QLatin1Char('?')) {
            const int end = findUnescaped(c, *cmd, 1);
            if (end == -1)
                return -1;
            sd.needle = cmd->mid(1, end - 1);
            cmd->remove(0, end + 1);
        } else {
            c = cmd->at(1);
            cmd->remove(0, 2);
            sd.needle = (c == QLatin1Char('&')) ? g.lastSubstitutePattern : g.lastSearch;
        }
        sd.forward = (c != QLatin1Char('?'));
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
        if (c == QLatin1Char('-') || c == QLatin1Char('+')) {
            if (n != 0)
                result = result + (add ? n - 1 : -(n - 1));
            add = c == QLatin1Char('+');
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

bool FakeVimHandler::Private::parseExCommmand(QString *line, ExCommand *cmd)
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
        if (c == QLatin1Char('\\')) {
            ++i; // skip escaped character
        } else if (close.isNull()) {
            if (c == QLatin1Char('|')) {
                // split on |
                break;
            } else if (c == QLatin1Char('/')) {
                subst = i > 0 && (line->at(i - 1) == QLatin1Char('s'));
                close = c;
            } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
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
    cmd->args = cmd->cmd.section(QRegExp(_("(?=[^a-zA-Z])")), 1);
    if (!cmd->args.isEmpty()) {
        cmd->cmd.chop(cmd->args.size());
        cmd->args = cmd->args.trimmed();

        // '!' at the end of command
        cmd->hasBang = cmd->args.startsWith(QLatin1Char('!'));
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
    line->remove(QRegExp(_("^\\s*(:+\\s*)*")));

    // special case ':!...' (use invalid range)
    if (line->startsWith(QLatin1Char('!'))) {
        cmd->range = Range();
        return true;
    }

    // FIXME: that seems to be different for %w and %s
    if (line->startsWith(QLatin1Char('%')))
        line->replace(0, 1, _("1,$"));

    int beginLine = parseLineAddress(line);
    int endLine;
    if (line->startsWith(QLatin1Char(','))) {
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
        const int beginLine = document()->findBlock(range->endPos).blockNumber() + 1;
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
    if (!cmd.matches(_("s"), _("substitute"))
        && !(cmd.cmd.isEmpty() && !cmd.args.isEmpty() && QString::fromLatin1("&~").contains(cmd.args[0]))) {
        return false;
    }

    int count = 1;
    QString line = cmd.args;
    const int countIndex = line.lastIndexOf(QRegExp(_("\\d+$")));
    if (countIndex != -1) {
        count = line.mid(countIndex).toInt();
        line = line.mid(0, countIndex).trimmed();
    }

    if (cmd.cmd.isEmpty()) {
        // keep previous substitution flags on '&&' and '~&'
        if (line.size() > 1 && line[1] == QLatin1Char('&'))
            g.lastSubstituteFlags += line.mid(2);
        else
            g.lastSubstituteFlags = line.mid(1);
        if (line[0] == QLatin1Char('~'))
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

    if (g.lastSubstituteFlags.contains(QLatin1Char('i')))
        needle.prepend(_("\\c"));

    QRegExp pattern = vimPatternToQtPattern(needle, hasConfig(ConfigIgnoreCase),
                                            hasConfig(ConfigSmartCase));

    QTextBlock lastBlock;
    QTextBlock firstBlock;
    const bool global = g.lastSubstituteFlags.contains(QLatin1Char('g'));
    for (int a = 0; a != count; ++a) {
        for (QTextBlock block = document()->findBlock(cmd.range.endPos);
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
        setTargetColumn();

        endEditBlock();
    }

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

    const QString lhs = args.section(QRegExp(_("\\s+")), 0, 0);
    const QString rhs = args.section(QRegExp(_("\\s+")), 1);
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
        case Map: // fall through
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
    if (!cmd.matches(_("his"), _("history")))
        return false;

    if (cmd.args.isEmpty()) {
        QString info;
        info += _("#  command history\n");
        int i = 0;
        foreach (const QString &item, g.commandBuffer.historyItems()) {
            ++i;
            info += QString::fromLatin1("%1 %2\n").arg(i, -8).arg(item);
        }
        emit q->extraInformationChanged(info);
    } else {
        notImplementedYet();
    }
    updateMiniBuffer();
    return true;
}

bool FakeVimHandler::Private::handleExRegisterCommand(const ExCommand &cmd)
{
    // :reg[isters] and :di[splay]
    if (!cmd.matches(_("reg"), _("registers")) && !cmd.matches(_("di"), _("display")))
        return false;

    QByteArray regs = cmd.args.toLatin1();
    if (regs.isEmpty()) {
        regs = "\"0123456789";
        QHashIterator<int, Register> it(g.registers);
        while (it.hasNext()) {
            it.next();
            if (it.key() > '9')
                regs += char(it.key());
        }
    }
    QString info;
    info += _("--- Registers ---\n");
    foreach (char reg, regs) {
        QString value = quoteUnprintable(registerContents(reg));
        info += QString::fromLatin1("\"%1   %2\n").arg(reg).arg(value);
    }
    emit q->extraInformationChanged(info);
    updateMiniBuffer();
    return true;
}

bool FakeVimHandler::Private::handleExSetCommand(const ExCommand &cmd)
{
    // :se[t]
    if (!cmd.matches(_("se"), _("set")))
        return false;

    clearMessage();
    QTC_CHECK(!cmd.args.isEmpty()); // Handled by plugin.

    if (cmd.args.contains(QLatin1Char('='))) {
        // Non-boolean config to set.
        int p = cmd.args.indexOf(QLatin1Char('='));
        QString error = theFakeVimSettings()
                ->trySetValue(cmd.args.left(p), cmd.args.mid(p + 1));
        if (!error.isEmpty())
            showMessage(MessageError, error);
    } else {
        QString optionName = cmd.args;

        bool toggleOption = optionName.endsWith(QLatin1Char('!'));
        bool printOption = !toggleOption && optionName.endsWith(QLatin1Char('?'));
        if (printOption || toggleOption)
            optionName.chop(1);

        bool negateOption = optionName.startsWith(_("no"));
        if (negateOption)
            optionName.remove(0, 2);

        SavedAction *act = theFakeVimSettings()->item(optionName);
        if (!act) {
            showMessage(MessageError, FakeVimHandler::tr("Unknown option:")
                        + QLatin1Char(' ') + cmd.args);
        } else if (act->defaultValue().type() == QVariant::Bool) {
            bool oldValue = act->value().toBool();
            if (printOption) {
                showMessage(MessageInfo, (oldValue ? _("") : _("no"))
                            + act->settingsKey().toLower());
            } else if (toggleOption || negateOption == oldValue) {
                act->setValue(!oldValue);
            }
        } else if (negateOption && !printOption) {
            showMessage(MessageError, FakeVimHandler::tr("Invalid argument:")
                        + QLatin1Char(' ') + cmd.args);
        } else if (toggleOption) {
            showMessage(MessageError, FakeVimHandler::tr("Trailing characters:")
                        + QLatin1Char(' ') + cmd.args);
        } else {
            showMessage(MessageInfo, act->settingsKey().toLower() + _("=")
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
    if (!cmd.matches(_("norm"), _("normal")))
        return false;
    //qDebug() << "REPLAY NORMAL: " << quoteUnprintable(reNormal.cap(3));
    replay(cmd.args);
    return true;
}

bool FakeVimHandler::Private::handleExYankDeleteCommand(const ExCommand &cmd)
{
    // :[range]d[elete] [x] [count]
    // :[range]y[ank] [x] [count]
    const bool remove = cmd.matches(_("d"), _("delete"));
    if (!remove && !cmd.matches(_("y"), _("yank")))
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
    if (!cmd.matches(_("c"), _("change")))
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
    if (!cmd.matches(_("m"), _("move")))
        return false;

    QString lineCode = cmd.args;

    const int startLine = document()->findBlock(cmd.range.beginPos).blockNumber();
    const int endLine = document()->findBlock(cmd.range.endPos).blockNumber();
    const int lines = endLine - startLine + 1;

    int targetLine = lineCode == _("0") ? -1 : parseLineAddress(&lineCode);
    if (targetLine >= startLine && targetLine < endLine) {
        showMessage(MessageError, FakeVimHandler::tr("Move lines into themselves."));
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
        insertText(QString::fromLatin1("\n"));
    }
    insertText(text);

    if (!insertAtEnd)
        moveUp(1);
    if (hasConfig(ConfigStartOfLine))
        moveToFirstNonBlankOnLine();

    // correct last selection
    leaveVisualMode();
    if (lastAnchor.line >= startLine && lastAnchor.line <= endLine)
        lastAnchor.line += targetLine - startLine + 1;
    if (lastPosition.line >= startLine && lastPosition.line <= endLine)
        lastPosition.line += targetLine - startLine + 1;
    setMark(QLatin1Char('<'), lastAnchor);
    setMark(QLatin1Char('>'), lastPosition);

    if (lines > 2)
        showMessage(MessageInfo, FakeVimHandler::tr("%n lines moved.", 0, lines));

    return true;
}

bool FakeVimHandler::Private::handleExJoinCommand(const ExCommand &cmd)
{
    // :[range]j[oin][!] [count]
    // FIXME: Argument [count] can follow immediately.
    if (!cmd.matches(_("j"), _("join")))
        return false;

    // get [count] from arguments
    bool ok;
    int count = cmd.args.toInt(&ok);

    if (ok) {
        setPosition(cmd.range.endPos);
    } else {
        setPosition(cmd.range.beginPos);
        const int startLine = document()->findBlock(cmd.range.beginPos).blockNumber();
        const int endLine = document()->findBlock(cmd.range.endPos).blockNumber();
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
    // :w, :x, :wq, ...
    //static QRegExp reWrite("^[wx]q?a?!?( (.*))?$");
    if (cmd.cmd != _("w") && cmd.cmd != _("x") && cmd.cmd != _("wq"))
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
    //const bool quit = prefix.contains(QLatin1Char('q')) || prefix.contains(QLatin1Char('x'));
    //const bool quitAll = quit && prefix.contains(QLatin1Char('a'));
    QString fileName = cmd.args;
    if (fileName.isEmpty())
        fileName = m_currentFileName;
    QFile file1(fileName);
    const bool exists = file1.exists();
    if (exists && !forced && !noArgs) {
        showMessage(MessageError, FakeVimHandler::tr
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
            showMessage(MessageError, FakeVimHandler::tr
               ("Cannot open file \"%1\" for writing").arg(fileName));
        }
        // Check result by reading back.
        QFile file3(fileName);
        file3.open(QIODevice::ReadOnly);
        QByteArray ba = file3.readAll();
        showMessage(MessageInfo, FakeVimHandler::tr("\"%1\" %2 %3L, %4C written.")
            .arg(fileName).arg(exists ? _(" ") : tr(" [New] "))
            .arg(ba.count('\n')).arg(ba.size()));
        //if (quitAll)
        //    passUnknownExCommand(forced ? "qa!" : "qa");
        //else if (quit)
        //    passUnknownExCommand(forced ? "q!" : "q");
    } else {
        showMessage(MessageError, FakeVimHandler::tr
            ("Cannot open file \"%1\" for reading").arg(fileName));
    }
    return true;
}

bool FakeVimHandler::Private::handleExReadCommand(const ExCommand &cmd)
{
    // :r[ead]
    if (!cmd.matches(_("r"), _("read")))
        return false;

    beginEditBlock();

    moveToStartOfLine();
    setTargetColumn();
    moveDown();
    int pos = position();

    m_currentFileName = cmd.args;
    QFile file(m_currentFileName);
    file.open(QIODevice::ReadOnly);
    QTextStream ts(&file);
    QString data = ts.readAll();
    insertText(data);

    setAnchorAndPosition(pos, pos);

    endEditBlock();

    showMessage(MessageInfo, FakeVimHandler::tr("\"%1\" %2L, %3C")
        .arg(m_currentFileName).arg(data.count(QLatin1Char('\n'))).arg(data.size()));

    return true;
}

bool FakeVimHandler::Private::handleExBangCommand(const ExCommand &cmd) // :!
{
    if (!cmd.cmd.isEmpty() || !cmd.hasBang)
        return false;

    bool replaceText = cmd.range.isValid();
    const QString command = QString(cmd.cmd.mid(1) + QLatin1Char(' ') + cmd.args).trimmed();
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
        showMessage(MessageInfo, FakeVimHandler::tr("%n lines filtered.", 0,
            input.count(QLatin1Char('\n'))));
    } else if (!result.isEmpty()) {
        emit q->extraInformationChanged(result);
    }

    return true;
}

bool FakeVimHandler::Private::handleExShiftCommand(const ExCommand &cmd)
{
    // :[range]{<|>}* [count]
    if (!cmd.cmd.isEmpty() || (!cmd.args.startsWith(QLatin1Char('<')) && !cmd.args.startsWith(QLatin1Char('>'))))
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
    if (c == QLatin1Char('<'))
        shiftRegionLeft(repeat);
    else
        shiftRegionRight(repeat);

    leaveVisualMode();

    return true;
}

bool FakeVimHandler::Private::handleExNohlsearchCommand(const ExCommand &cmd)
{
    // :noh, :nohl, ..., :nohlsearch
    if (cmd.cmd.size() < 3 || !QString(_("nohlsearch")).startsWith(cmd.cmd))
        return false;

    g.highlightsCleared = true;
    updateHighlights();
    return true;
}

bool FakeVimHandler::Private::handleExUndoRedoCommand(const ExCommand &cmd)
{
    // :undo
    // :redo
    bool undo = (cmd.cmd == _("u") || cmd.cmd == _("un") || cmd.cmd == _("undo"));
    if (!undo && cmd.cmd != _("red") && cmd.cmd != _("redo"))
        return false;

    undoRedo(undo);
    updateMiniBuffer();
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
    if (cmd.cmd != _("so") && cmd.cmd != _("source"))
        return false;

    QString fileName = cmd.args;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        showMessage(MessageError, FakeVimHandler::tr("Cannot open file %1").arg(fileName));
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
            while (parseExCommmand(&commandLine, &cmd)) {
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
    if (cmd.cmd != _("echo"))
        return false;
    showMessage(MessageInfo, cmd.args);
    return true;
}

void FakeVimHandler::Private::handleExCommand(const QString &line0)
{
    QString line = line0; // Make sure we have a copy to prevent aliasing.

    if (line.endsWith(QLatin1Char('%'))) {
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
    while (parseExCommmand(&line, &cmd)) {
        if (!handleExCommandHelper(cmd)) {
            showMessage(MessageError, tr("Not an editor command: %1").arg(lastCommand));
            break;
        }
        lastCommand = line;
    }

    // if the last command closed the editor, we would crash here (:vs and then :on)
    if (!(m_textedit || m_plaintextedit))
        return;

    endEditBlock();

    resetCommandMode();
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
        || handleExSourceCommand(cmd)
        || handleExSubstituteCommand(cmd)
        || handleExWriteCommand(cmd)
        || handleExEchoCommand(cmd);
}

bool FakeVimHandler::Private::handleExPluginCommand(const ExCommand &cmd)
{
    bool handled = false;
    int pos = m_cursor.position();
    commitCursor();
    emit q->handleExCommandRequested(&handled, cmd);
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
        QChar c = document()->characterAt(pos);
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
    QRegExp needleExp = vimPatternToQtPattern(sd.needle, hasConfig(ConfigIgnoreCase),
                                              hasConfig(ConfigSmartCase));
    if (!needleExp.isValid()) {
        if (showMessages) {
            QString error = needleExp.errorString();
            showMessage(MessageError,
                        FakeVimHandler::tr("Invalid regular expression: %1").arg(error));
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
                        FakeVimHandler::tr("Pattern not found: %1").arg(sd.needle));
                }
            } else if (showMessages) {
                QString msg = sd.forward
                    ? FakeVimHandler::tr("Search hit BOTTOM, continuing at TOP.")
                    : FakeVimHandler::tr("Search hit TOP, continuing at BOTTOM.");
                showMessage(MessageWarning, msg);
            }
        } else if (showMessages) {
            QString msg = sd.forward
                ? FakeVimHandler::tr("Search hit BOTTOM without match for: %1")
                : FakeVimHandler::tr("Search hit TOP without match for: %1");
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
    QTextDocument *doc = tc->document();
    const QTextBlock block = tc->block();
    const int maxPos = block.position() + block.length() - 1;
    int i = tc->position();
    while (doc->characterAt(i).isSpace() && i < maxPos)
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
    setDotCommand(_("%1=="), endLine - beginLine + 1);
    endEditBlock();

    const int lines = endLine - beginLine + 1;
    if (lines > 2)
        showMessage(MessageInfo, FakeVimHandler::tr("%n lines indented.", 0, lines));
}

void FakeVimHandler::Private::indentText(const Range &range, QChar typedChar)
{
    int beginBlock = document()->findBlock(range.beginPos).blockNumber();
    int endBlock = document()->findBlock(range.endPos).blockNumber();
    if (beginBlock > endBlock)
        std::swap(beginBlock, endBlock);

    // Don't remember current indentation in last text insertion.
    const QString lastInsertion = m_buffer->lastInsertion;
    emit q->indentRegion(beginBlock, endBlock, typedChar);
    m_buffer->lastInsertion = lastInsertion;
}

bool FakeVimHandler::Private::isElectricCharacter(QChar c) const
{
    bool result = false;
    emit q->checkForElectricCharacter(&result, c);
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
    setTargetColumn();

    const int lines = endLine - beginLine + 1;
    if (lines > 2) {
        showMessage(MessageInfo,
            FakeVimHandler::tr("%n lines %1ed %2 time.", 0, lines)
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
        //int old = (c.isLetterOrNumber() || c.unicode() == QLatin1Char('_')) ? 2
        //    :  c.isSpace() ? 0 : 1;
        //qDebug() << c.unicode() << old << m_charClass[c.unicode()];
        return m_charClass[c.unicode()];
    }
    if (c.isLetterOrNumber() || c.unicode() == QLatin1Char('_'))
        return 2;
    return c.isSpace() ? 0 : 1;
}

void FakeVimHandler::Private::miniBufferTextEdited(const QString &text, int cursorPos,
    int anchorPos)
{
    if (g.subsubmode != SearchSubSubMode && g.mode != ExMode) {
        editor()->setFocus();
    } else if (text.isEmpty()) {
        // editing cancelled
        enterFakeVim();
        handleDefaultKey(Input(Qt::Key_Escape, Qt::NoModifier, QString()));
        leaveFakeVim();

        editor()->setFocus();
        updateCursorShape();
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
            emit q->commandBufferChanged(buffer, pos, anchor, 0, q);
        // update search expression
        if (g.subsubmode == SearchSubSubMode) {
            updateFind(false);
            exportSelection();
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
}

// Helper to parse a-z,A-Z,48-57,_
static int someInt(const QString &str)
{
    if (str.toInt())
        return str.toInt();
    if (str.size())
        return str.at(0).unicode();
    return 0;
}

void FakeVimHandler::Private::setupCharClass()
{
    for (int i = 0; i < 256; ++i) {
        const QChar c = QChar(QLatin1Char(i));
        m_charClass[i] = c.isSpace() ? 0 : 1;
    }
    const QString conf = config(ConfigIsKeyword).toString();
    foreach (const QString &part, conf.split(QLatin1Char(','))) {
        if (part.contains(QLatin1Char('-'))) {
            const int from = someInt(part.section(QLatin1Char('-'), 0, 0));
            const int to = someInt(part.section(QLatin1Char('-'), 1, 1));
            for (int i = qMax(0, from); i <= qMin(255, to); ++i)
                m_charClass[i] = 2;
        } else {
            m_charClass[qMin(255, someInt(part))] = 2;
        }
    }
}

void FakeVimHandler::Private::moveToBoundary(bool simple, bool forward)
{
    QTextDocument *doc = document();
    QTextCursor tc(doc);
    tc.setPosition(position());
    if (forward ? tc.atBlockEnd() : tc.atBlockStart())
        return;

    QChar c = document()->characterAt(tc.position() + (forward ? -1 : 1));
    int lastClass = tc.atStart() ? -1 : charClass(c, simple);
    QTextCursor::MoveOperation op = forward ? Right : Left;
    while (true) {
        c = doc->characterAt(tc.position());
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
    moveToNextWord(false, count, simple, forward, emptyLines);
}

void FakeVimHandler::Private::moveToNextWordEnd(int count, bool simple, bool forward, bool emptyLines)
{
    moveToNextWord(true, count, simple, forward, emptyLines);
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
    QTextDocument *doc = document();
    const int d = forward ? 1 : -1;
    // FIXME: This also depends on whether 'cpositions' Vim option contains ';'.
    const int skip = (repeats && repeat == 1 && exclusive) ? d : 0;
    int pos = position() + d + skip;

    for (; repeat > 0 && (forward ? pos < n : pos > n); pos += d) {
        if (doc->characterAt(pos).unicode() == key0)
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
    static const QString parenthesesChars(_("([{}])"));
    while (!parenthesesChars.contains(document()->characterAt(tc.position())) && !tc.atBlockEnd())
        tc.setPosition(tc.position() + 1);

    if (tc.atBlockEnd())
        tc = m_cursor;

    emit q->moveToMatchingParenthesis(&moved, &forward, &tc);
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
    return document()->findBlock(qMin(anchor(), position())).blockNumber();
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
        //if (c == QLatin1Char(' '))
        //    ++logical;
        //else
        if (c == QLatin1Char('\t'))
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
        if (c == QLatin1Char('\t'))
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
    if (range.rangemode == RangeCharMode) {
        QTextCursor tc(document());
        tc.setPosition(range.beginPos, MoveAnchor);
        tc.setPosition(range.endPos, KeepAnchor);
        return tc.selection().toPlainText();
    }
    if (range.rangemode == RangeLineMode) {
        QTextCursor tc(document());
        int firstPos = firstPositionInLine(lineForPosition(range.beginPos));
        int lastLine = lineForPosition(range.endPos);
        bool endOfDoc = lastLine == lineNumber(document()->lastBlock());
        int lastPos = endOfDoc ? lastPositionInDocument(true) : firstPositionInLine(lastLine + 1);
        tc.setPosition(firstPos, MoveAnchor);
        tc.setPosition(lastPos, KeepAnchor);
        return tc.selection().toPlainText() + _(endOfDoc? "\n" : "");
    }
    // FIXME: Performance?
    int beginLine = lineForPosition(range.beginPos);
    int endLine = lineForPosition(range.endPos);
    int beginColumn = 0;
    int endColumn = INT_MAX;
    if (range.rangemode == RangeBlockMode) {
        int column1 = range.beginPos - firstPositionInLine(beginLine);
        int column2 = range.endPos - firstPositionInLine(endLine);
        beginColumn = qMin(column1, column2);
        endColumn = qMax(column1, column2);
    }
    int len = endColumn - beginColumn + 1;
    QString contents;
    QTextBlock block = document()->findBlockByLineNumber(beginLine - 1);
    for (int i = beginLine; i <= endLine && block.isValid(); ++i) {
        QString line = block.text();
        if (range.rangemode == RangeBlockMode) {
            line = line.mid(beginColumn, len);
            if (line.size() < len)
                line += QString(len - line.size(), QLatin1Char(' '));
        }
        contents += line;
        if (!contents.endsWith(QLatin1Char('\n')))
            contents += QLatin1Char('\n');
        block = block.next();
    }
    //qDebug() << "SELECTED: " << contents;
    return contents;
}

void FakeVimHandler::Private::yankText(const Range &range, int reg)
{
    const QString text = selectText(range);
    setRegister(reg, text, range.rangemode);

    // If register is not specified or " ...
    if (m_register == '"') {
        // copy to yank register 0 too
        setRegister('0', text, range.rangemode);

        // with delete and change commands set register 1 (if text contains more lines) or
        // small delete register -
        if (g.submode == DeleteSubMode || g.submode == ChangeSubMode) {
            if (text.contains(QLatin1Char('\n')))
                setRegister('1', text, range.rangemode);
            else
                setRegister('-', text, range.rangemode);
        }
    } else {
        // Always copy to " register too.
        setRegister('"', text, range.rangemode);
    }

    const int lines = document()->findBlock(range.endPos).blockNumber()
        - document()->findBlock(range.beginPos).blockNumber() + 1;
    if (lines > 2)
        showMessage(MessageInfo, FakeVimHandler::tr("%n lines yanked.", 0, lines));
}

void FakeVimHandler::Private::transformText(const Range &range,
    Transformation transformFunc, const QVariant &extra)
{
    QTextCursor tc = m_cursor;
    int posAfter = range.beginPos;
    switch (range.rangemode) {
        case RangeCharMode: {
            // This can span multiple lines.
            beginEditBlock();
            tc.setPosition(range.beginPos, MoveAnchor);
            tc.setPosition(range.endPos, KeepAnchor);
            TransformationData td(tc.selectedText(), extra);
            (this->*transformFunc)(&td);
            insertText(tc, td.to);
            endEditBlock();
            break;
        }
        case RangeLineMode:
        case RangeLineModeExclusive: {
            beginEditBlock();
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
            TransformationData td(tc.selectedText(), extra);
            (this->*transformFunc)(&td);
            posAfter = tc.anchor();
            insertText(tc, td.to);
            endEditBlock();
            break;
        }
        case RangeBlockAndTailMode:
        case RangeBlockMode: {
            int beginLine = lineForPosition(range.beginPos);
            int endLine = lineForPosition(range.endPos);
            int column1 = range.beginPos - firstPositionInLine(beginLine);
            int column2 = range.endPos - firstPositionInLine(endLine);
            int beginColumn = qMin(column1, column2);
            int endColumn = qMax(column1, column2);
            if (range.rangemode == RangeBlockAndTailMode)
                endColumn = INT_MAX - 1;
            QTextBlock block = document()->findBlockByLineNumber(endLine - 1);
            beginEditBlock();
            for (int i = beginLine; i <= endLine && block.isValid(); ++i) {
                int bCol = qMin(beginColumn, block.length() - 1);
                int eCol = qMin(endColumn + 1, block.length() - 1);
                tc.setPosition(block.position() + bCol, MoveAnchor);
                tc.setPosition(block.position() + eCol, KeepAnchor);
                TransformationData td(tc.selectedText(), extra);
                (this->*transformFunc)(&td);
                insertText(tc, td.to);
                block = block.previous();
            }
            endEditBlock();
            break;
        }
    }

    setPosition(posAfter);
    setTargetColumn();
}

void FakeVimHandler::Private::insertText(QTextCursor &tc, const QString &text)
{
  if (hasConfig(ConfigPassKeys)) {
      QTextCursor oldTc = m_cursor;
      m_cursor = tc;

      if (tc.hasSelection() && text.isEmpty()) {
          QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, QString());
          passEventToEditor(event);
      }

      foreach (QChar c, text) {
          QKeyEvent event(QEvent::KeyPress, -1, Qt::NoModifier, QString(c));
          passEventToEditor(event);
      }

      tc = m_cursor;
      m_cursor = oldTc;
  } else {
      tc.insertText(text);
  }
}

void FakeVimHandler::Private::insertText(const Register &reg)
{
    QTC_ASSERT(reg.rangemode == RangeCharMode,
        qDebug() << "WRONG INSERT MODE: " << reg.rangemode; return);
    setAnchor();
    m_cursor.insertText(reg.contents);
    //dump("AFTER INSERT");
}

void FakeVimHandler::Private::removeText(const Range &range)
{
    //qDebug() << "REMOVE: " << range;
    transformText(range, &FakeVimHandler::Private::removeTransform);
}

void FakeVimHandler::Private::removeTransform(TransformationData *td)
{
    Q_UNUSED(td);
}

void FakeVimHandler::Private::downCase(const Range &range)
{
    transformText(range, &FakeVimHandler::Private::downCaseTransform);
}

void FakeVimHandler::Private::downCaseTransform(TransformationData *td)
{
    td->to = td->from.toLower();
}

void FakeVimHandler::Private::upCase(const Range &range)
{
    transformText(range, &FakeVimHandler::Private::upCaseTransform);
}

void FakeVimHandler::Private::upCaseTransform(TransformationData *td)
{
    td->to = td->from.toUpper();
}

void FakeVimHandler::Private::invertCase(const Range &range)
{
    transformText(range, &FakeVimHandler::Private::invertCaseTransform);
}

void FakeVimHandler::Private::invertCaseTransform(TransformationData *td)
{
    foreach (QChar c, td->from)
        td->to += c.isUpper() ? c.toLower() : c.toUpper();
}

void FakeVimHandler::Private::replaceText(const Range &range, const QString &str)
{
    Transformation tr = &FakeVimHandler::Private::replaceByStringTransform;
    transformText(range, tr, str);
}

void FakeVimHandler::Private::replaceByStringTransform(TransformationData *td)
{
    td->to = td->extraData.toString();
}

void FakeVimHandler::Private::replaceByCharTransform(TransformationData *td)
{
    // Replace each character but preserve lines.
    const int len = td->from.size();
    td->to = QString(len, td->extraData.toChar());
    for (int i = 0; i < len; ++i) {
        if (td->from.at(i) == ParagraphSeparator)
            td->to[i] = ParagraphSeparator;
    }
}

void FakeVimHandler::Private::pasteText(bool afterCursor)
{
    const QString text = registerContents(m_register);
    const RangeMode rangeMode = registerRangeMode(m_register);

    beginEditBlock();

    // In visual mode paste text only inside selection.
    bool pasteAfter = isVisualMode() ? false : afterCursor;

    bool visualCharMode = isVisualCharMode();
    if (visualCharMode) {
        leaveVisualMode();
        g.rangemode = RangeCharMode;
        Range range = currentRange();
        range.endPos++;
        yankText(range, m_register);
        removeText(range);
    } else if (isVisualLineMode()) {
        leaveVisualMode();
        g.rangemode = RangeLineMode;
        Range range = currentRange();
        range.endPos++;
        yankText(range, m_register);
        removeText(range);
        handleStartOfLine();
    } else if (isVisualBlockMode()) {
        leaveVisualMode();
        g.rangemode = RangeBlockMode;
        Range range = currentRange();
        yankText(range, m_register);
        removeText(range);
        setPosition(qMin(position(), anchor()));
    }

    switch (rangeMode) {
        case RangeCharMode: {
            m_targetColumn = 0;
            const int pos = position() + 1;
            if (pasteAfter && rightDist() > 0)
                moveRight();
            insertText(text.repeated(count()));
            if (text.contains(QLatin1Char('\n')))
                setPosition(pos);
            else
                moveLeft();
            break;
        }
        case RangeLineMode:
        case RangeLineModeExclusive: {
            QTextCursor tc = m_cursor;
            if (visualCharMode)
                tc.insertBlock();
            else
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
            const QStringList lines = text.split(QLatin1Char('\n'));
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
                    tc.insertText(QString(col - length + 1, QLatin1Char(' ')));
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
            while (characterAtCursor() == QLatin1Char(' ') || characterAtCursor() == QLatin1Char('\t'))
                moveRight();
            m_cursor.insertText(QString(QLatin1Char(' ')));
        }
    }
    setPosition(pos);
}

void FakeVimHandler::Private::insertNewLine()
{
    if ( m_buffer->editBlockLevel <= 1 && hasConfig(ConfigPassKeys) ) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QLatin1String("\n"));
        if (passEventToEditor(event))
            return;
    }

    insertText(QString::fromLatin1("\n"));
    insertAutomaticIndentation(true);
}

bool FakeVimHandler::Private::handleInsertInEditor(const Input &input)
{
    if (m_buffer->editBlockLevel > 0 || !hasConfig(ConfigPassKeys))
        return false;

    joinPreviousEditBlock();

    QKeyEvent event(QEvent::KeyPress, input.key(), input.modifiers(), input.text());
    setAnchor();
    if (!passEventToEditor(event))
        return !m_textedit && !m_plaintextedit; // Mark event as handled if it has destroyed editor.

    endEditBlock();

    return true;
}

bool FakeVimHandler::Private::passEventToEditor(QEvent &event)
{
    removeEventFilter();

    EDITOR(setOverwriteMode(false));
    commitCursor();
    bool accepted = QApplication::sendEvent(editor(), &event);
    if (!m_textedit && !m_plaintextedit)
        return false;
    updateCursorShape();

    if (accepted)
        pullCursor();

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
    return tc.selectedText().replace(ParagraphSeparator, QLatin1Char('\n'));
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

QTextBlock FakeVimHandler::Private::nextLine(const QTextBlock &block) const
{
    return document()->findBlock(block.position() + block.length());
}

QTextBlock FakeVimHandler::Private::previousLine(const QTextBlock &block) const
{
    return document()->findBlock(block.position() - 1);
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
    const QTextBlock block = document()->findBlock(pos);
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
        updateMiniBuffer();
    }
}

void FakeVimHandler::Private::leaveVisualMode()
{
    if (!isVisualMode())
        return;

    setMark(QLatin1Char('<'), markLessPosition());
    setMark(QLatin1Char('>'), markGreaterPosition());
    m_buffer->lastVisualModeInverted = anchor() > position();
    if (isVisualLineMode()) {
        g.rangemode = RangeLineMode;
        g.movetype = MoveLineWise;
    } else if (isVisualCharMode()) {
        g.rangemode = RangeCharMode;
        g.movetype = MoveInclusive;
    } else if (isVisualBlockMode()) {
        g.rangemode = RangeBlockMode;
        g.movetype = MoveInclusive;
    }

    g.visualMode = NoVisualMode;
    updateMiniBuffer();
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
        tc.insertText(_("X"));
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
    QTC_ASSERT(m_buffer->editBlockLevel > 0,
        qDebug() << "beginEditBlock() not called before endEditBlock()!"; return);
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
    if (isInsertMode() && (charsAdded > 0 || charsRemoved > 0)) {
        BufferData::InsertState &insertState = m_buffer->insertState;
        if (!isInsertStateValid()) {
            insertState.pos1 = m_oldInternalPosition;
            g.dotCommand = _("i");
            resetCount();
        }

        // Ignore changes outside inserted text (e.g. renaming other occurrences of a variable).
        if (position + charsRemoved >= insertState.pos1 && position <= insertState.pos2) {
            if (charsRemoved > 0) {
                if (position < insertState.pos1) {
                    // backspaces
                    const int bs = insertState.pos1 - position;
                    const QString inserted = textAt(position, m_oldInternalPosition);
                    const QString removed = insertState.textBeforeCursor.right(bs);
                    // Ignore backspaces if same text was just inserted.
                    if ( !inserted.endsWith(removed) ) {
                        insertState.backspaces += bs;
                        insertState.pos1 = position;
                        insertState.pos2 = qMax(position, insertState.pos2 - bs);
                    }
                } else if (position + charsRemoved > insertState.pos2) {
                    // deletes
                    insertState.deletes += position + charsRemoved - insertState.pos2;
                }
            } else if (charsAdded > 0 && insertState.insertingSpaces) {
                for (int i = position; i < position + charsAdded; ++i) {
                    const QChar c = document()->characterAt(i);
                    if (c.unicode() == ' ' || c.unicode() == '\t')
                        insertState.spaces.insert(i);
                }
            }

            insertState.pos2 = qMax(insertState.pos2 + charsAdded - charsRemoved,
                                      position + charsAdded);
            m_oldInternalPosition = position + charsAdded;
            insertState.textBeforeCursor = textAt(document()->findBlock(m_oldInternalPosition).position(),
                                            m_oldInternalPosition);
        }
    }

    if (!m_highlighted.isEmpty())
        emit q->highlightMatches(m_highlighted);
}

void FakeVimHandler::Private::onUndoCommandAdded()
{
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
        const QString msg = undo ? FakeVimHandler::tr("Already at oldest change.")
            : FakeVimHandler::tr("Already at newest change.");
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
        m_buffer->lastChangePosition = state.position;
        Marks marks = m_buffer->marks;
        marks.swap(state.marks);
        updateMarks(marks);
        m_buffer->lastVisualMode = state.lastVisualMode;
        m_buffer->lastVisualModeInverted = state.lastVisualModeInverted;
        setMark(QLatin1Char('\''), lastPos);
        setMark(QLatin1Char('`'), lastPos);
        setCursorPosition(m_buffer->lastChangePosition);
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
    bool thinCursor = g.mode == ExMode
            || g.subsubmode == SearchSubSubMode
            || g.mode == InsertMode
            || (isVisualMode() && !isVisualCharMode());
    EDITOR(setOverwriteMode(!thinCursor));
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
    QTC_ASSERT(mode == InsertMode || mode == ReplaceMode, return);
    if (g.mode == mode)
        return;

    if (mode == InsertMode && g.returnToMode != InsertMode) {
        // If entering insert mode from command mode, m_targetColumn shouldn't be -1 (end of line).
        if (m_targetColumn == -1)
            setTargetColumn();
    }

    g.mode = mode;
    g.submode = NoSubMode;
    g.subsubmode = NoSubSubMode;
    g.returnToMode = mode;
    clearLastInsertion();
}

void FakeVimHandler::Private::enterVisualInsertMode(QChar command)
{
    if (isVisualBlockMode()) {
        bool append = command == QLatin1Char('A');
        bool change = command == QLatin1Char('s') || command == QLatin1Char('c');

        setDotCommand(visualDotCommand() + QString::number(count()) + command);
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
            pushUndoState();
            beginEditBlock();
            Range range(position(), anchor(), RangeBlockMode);
            yankText(range, m_register);
            removeText(range);
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
        if (command == QLatin1Char('I')) {
            setDotCommand(_("%1i"), count());
            if (lineForPosition(anchor()) <= lineForPosition(position())) {
                setPosition(qMin(anchor(), position()));
                moveToStartOfLine();
            }
        } else if (command == QLatin1Char('A')) {
            setDotCommand(_("%1a"), count());
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
    if (isNoVisualMode() && atEndOfLine())
        moveLeft();
    g.mode = CommandMode;
    clearCommandMode();
    g.returnToMode = returnToMode;
}

void FakeVimHandler::Private::enterExMode(const QString &contents)
{
    g.currentMessage.clear();
    g.commandBuffer.clear();
    if (isVisualMode())
        g.commandBuffer.setContents(QString::fromLatin1("'<,'>") + contents, contents.size() + 5);
    else
        g.commandBuffer.setContents(contents, contents.size());
    g.mode = ExMode;
    g.submode = NoSubMode;
    g.subsubmode = NoSubSubMode;
}

void FakeVimHandler::Private::recordJump(int position)
{
    CursorPosition pos = position >= 0 ? CursorPosition(document(), position)
                                       : CursorPosition(m_cursor);
    setMark(QLatin1Char('\''), pos);
    setMark(QLatin1Char('`'), pos);
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
    setMark(QLatin1Char('\''), m);
    setMark(QLatin1Char('`'), m);
    for (int i = 0; i < len; ++i) {
        to.push(m);
        setCursorPosition(from.top());
        from.pop();
    }
}

Column FakeVimHandler::Private::indentation(const QString &line) const
{
    int ts = config(ConfigTabStop).toInt();
    int physical = 0;
    int logical = 0;
    int n = line.size();
    while (physical < n) {
        QChar c = line.at(physical);
        if (c == QLatin1Char(' '))
            ++logical;
        else if (c == QLatin1Char('\t'))
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
        return QString(n, QLatin1Char(' '));
    return QString(n / ts, QLatin1Char('\t'))
         + QString(n % ts, QLatin1Char(' '));
}

void FakeVimHandler::Private::insertAutomaticIndentation(bool goingDown, bool forceAutoIndent)
{
    if (!forceAutoIndent && !hasConfig(ConfigAutoIndent) && !hasConfig(ConfigSmartIndent))
        return;

    if (hasConfig(ConfigSmartIndent)) {
        QTextBlock bl = block();
        Range range(bl.position(), bl.position());
        indentText(range, QLatin1Char('\n'));
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
    clearCommandMode();
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
        command = _("v");
    else if (isVisualLineMode())
        command = _("V");
    else if (isVisualBlockMode())
        command = _("<c-v>");
    else
        return QString();

    const int down = qAbs(start.blockNumber() - end.blockNumber());
    if (down != 0)
        command.append(QString::fromLatin1("%1j").arg(down));

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
        moveRight();
        if (atEndOfLine())
            moveRight();
    } else {
        moveLeft();
        if (atBlockStart())
            moveLeft();
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
            QChar afterCursor = document()->characterAt(position() + direction);
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
                while (pos >= min && document()->characterAt(--pos).isSpace()) {}
                if (pos >= min)
                    setAnchorAndPosition(pos + 1, position());
            }

            if (i + 1 < repeat) {
                if (forward) {
                    moveRight();
                    if (atEndOfLine())
                        moveRight();
                } else {
                    moveLeft();
                    if (atBlockStart())
                        moveLeft();
                }
            }
        }
    }

    if (inner) {
        g.movetype = MoveInclusive;
    } else {
        g.movetype = MoveExclusive;
        if (isNoVisualMode()) {
            moveRight();
            if (atEndOfLine())
                moveRight();
        } else if (isVisualLineMode()) {
            g.visualMode = VisualCharMode;
        }
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
    Q_UNUSED(inner);
}

void FakeVimHandler::Private::selectParagraphTextObject(bool inner)
{
    Q_UNUSED(inner);
}

bool FakeVimHandler::Private::selectBlockTextObject(bool inner,
    char left, char right)
{
    QString sleft = QString(QLatin1Char(left));
    QString sright = QString(QLatin1Char(right));

    int p1 = blockBoundary(sleft, sright, false, count());
    if (p1 == -1)
        return false;

    int p2 = blockBoundary(sleft, sright, true, count());
    if (p2 == -1)
        return false;

    if (inner)
        p1 += sleft.size();
    else
        p2 -= sright.size() - 2;

    if (isVisualMode())
        --p2;

    setAnchorAndPosition(p1, p2);
    g.movetype = MoveExclusive;

    return true;
}

bool FakeVimHandler::Private::changeNumberTextObject(int count)
{
    const QTextBlock block = this->block();
    const QString lineText = block.text();
    const int posMin = m_cursor.positionInBlock() + 1;

    // find first decimal, hexadecimal or octal number under or after cursor position
    QRegExp re(_("(0[xX])(0*[0-9a-fA-F]+)|(0)(0*[0-7]+)(?=\\D|$)|(\\d+)"));
    int pos = 0;
    while ((pos = re.indexIn(lineText, pos)) != -1 && pos + re.matchedLength() < posMin)
        ++pos;
    if (pos == -1)
        return false;
    int len = re.matchedLength();
    QString prefix = re.cap(1) + re.cap(3);
    bool hex = prefix.length() >= 2 && (prefix[1].toLower() == QLatin1Char('x'));
    bool octal = !hex && !prefix.isEmpty();
    const QString num = hex ? re.cap(2) : octal ? re.cap(4) : re.cap(5);

    // parse value
    bool ok;
    int base = hex ? 16 : octal ? 8 : 10;
    qlonglong value = 0;  // decimal value
    qlonglong uvalue = 0; // hexadecimal or octal value (only unsigned)
    if (hex || octal)
        uvalue = num.toULongLong(&ok, base);
    else
        value = num.toLongLong(&ok, base);
    QTC_ASSERT(ok, qDebug() << "Cannot parse number:" << num << "base:" << base; return false);

    // negative decimal number
    if (!octal && !hex && pos > 0 && lineText[pos - 1] == QLatin1Char('-')) {
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
        const int lastLetter = num.lastIndexOf(QRegExp(_("[a-fA-F]")));
        if (lastLetter != -1 && num[lastLetter].isUpper())
            repl = repl.toUpper();
    }

    // preserve leading zeroes
    if ((octal || hex) && repl.size() < num.size())
        prefix.append(QString::fromLatin1("0").repeated(num.size() - repl.size()));
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
        if (document()->characterAt(p1) == ParagraphSeparator)
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
        if (code == QLatin1Char('<'))
            return CursorPosition(document(), qMin(anchor(), position()));
        if (code == QLatin1Char('>'))
            return CursorPosition(document(), qMax(anchor(), position()));
    }
    if (code == QLatin1Char('.'))
        return m_buffer->lastChangePosition;
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
        emit q->jumpToGlobalMark(mark, backTickMode, m.fileName());
        return false;
    }

    if ((mark == QLatin1Char('\'') || mark == QLatin1Char('`')) && !m_buffer->jumpListUndo.isEmpty())
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
    for (MarksIterator it(newMarks); it.hasNext(); ) {
        it.next();
        m_buffer->marks[it.key()] = it.value();
    }
}

RangeMode FakeVimHandler::Private::registerRangeMode(int reg) const
{
    bool isClipboard;
    bool isSelection;
    getRegisterType(reg, &isClipboard, &isSelection);

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
        return (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char('\r'))) ? RangeLineMode : RangeCharMode;
    }

    return g.registers[reg].rangemode;
}

void FakeVimHandler::Private::setRegister(int reg, const QString &contents, RangeMode mode)
{
    bool copyToClipboard;
    bool copyToSelection;
    getRegisterType(reg, &copyToClipboard, &copyToSelection);

    QString contents2 = contents;
    if (mode == RangeLineMode && !contents2.endsWith(QLatin1Char('\n')))
        contents2.append(QLatin1Char('\n'));

    if (copyToClipboard || copyToSelection) {
        if (copyToClipboard)
            setClipboardData(contents2, mode, QClipboard::Clipboard);
        if (copyToSelection)
            setClipboardData(contents2, mode, QClipboard::Selection);
    } else {
        g.registers[reg].contents = contents2;
        g.registers[reg].rangemode = mode;
    }
}

QString FakeVimHandler::Private::registerContents(int reg) const
{
    bool copyFromClipboard;
    bool copyFromSelection;
    getRegisterType(reg, &copyFromClipboard, &copyFromSelection);

    if (copyFromClipboard || copyFromSelection) {
        QClipboard *clipboard = QApplication::clipboard();
        if (copyFromClipboard)
            return clipboard->text(QClipboard::Clipboard);
        if (copyFromSelection)
            return clipboard->text(QClipboard::Selection);
    }

    return g.registers[reg].contents;
}

void FakeVimHandler::Private::getRegisterType(int reg, bool *isClipboard, bool *isSelection) const
{
    bool clipboard = false;
    bool selection = false;

    if (reg == QLatin1Char('"')) {
        QStringList list = config(ConfigClipboard).toString().split(QLatin1Char(','));
        clipboard = list.contains(_("unnamedplus"));
        selection = list.contains(_("unnamed"));
    } else if (reg == QLatin1Char('+')) {
        clipboard = true;
    } else if (reg == QLatin1Char('*')) {
        selection = true;
    }

    // selection (primary) is clipboard on systems without selection support
    if (selection && !QApplication::clipboard()->supportsSelection()) {
        clipboard = true;
        selection = false;
    }

    if (isClipboard != 0)
        *isClipboard = clipboard;
    if (isSelection != 0)
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
    d->m_textedit = 0;
    d->m_plaintextedit = 0;
}

bool FakeVimHandler::eventFilter(QObject *ob, QEvent *ev)
{
#ifndef FAKEVIM_STANDALONE
    if (!theFakeVimSetting(ConfigUseFakeVim)->value().toBool())
        return QObject::eventFilter(ob, ev);
#endif

    // Catch mouse events on the viewport.
    QWidget *viewport = 0;
    if (d->m_plaintextedit)
        viewport = d->m_plaintextedit->viewport();
    else if (d->m_textedit)
        viewport = d->m_textedit->viewport();
    if (ob == viewport) {
        if (ev->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mev = static_cast<QMouseEvent *>(ev);
            if (mev->button() == Qt::LeftButton) {
                d->importSelection();
                //return true;
            }
        }
        if (ev->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mev = static_cast<QMouseEvent *>(ev);
            if (mev->button() == Qt::LeftButton)
                Private::g.visualMode = NoVisualMode;
        }
        return QObject::eventFilter(ob, ev);
    }

    if (ev->type() == QEvent::Shortcut) {
        d->passShortcuts(false);
        return false;
    }

    if (ev->type() == QEvent::InputMethod && ob == d->editor()) {
        // This handles simple dead keys. The sequence of events is
        // KeyRelease-InputMethod-KeyRelease  for dead keys instead of
        // KeyPress-KeyRelease as for simple keys. As vi acts on key presses,
        // we have to act on the InputMethod event.
        // FIXME: A first approximation working for e.g. ^ on a German keyboard
        QInputMethodEvent *imev = static_cast<QInputMethodEvent *>(ev);
        KEY_DEBUG("INPUTMETHOD" << imev->commitString() << imev->preeditString());
        QString commitString = imev->commitString();
        int key = commitString.size() == 1 ? commitString.at(0).unicode() : 0;
        QKeyEvent kev(QEvent::KeyPress, key, Qt::KeyboardModifiers(), commitString);
        EventResult res = d->handleEvent(&kev);
        return res == EventHandled || res == EventCancelled;
    }

    if (ev->type() == QEvent::KeyPress &&
        (ob == d->editor()
         || (Private::g.mode == ExMode || Private::g.subsubmode == SearchSubSubMode))) {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
        KEY_DEBUG("KEYPRESS" << kev->key() << kev->text() << QChar(kev->key()));
        EventResult res = d->handleEvent(kev);
        //if (Private::g.mode == InsertMode)
        //    emit completionRequested();
        // returning false core the app see it
        //KEY_DEBUG("HANDLED CODE:" << res);
        //return res != EventPassedToCore;
        //return true;
        return res == EventHandled || res == EventCancelled;
    }

    if (ev->type() == QEvent::ShortcutOverride && ob == d->editor()) {
        QKeyEvent *kev = static_cast<QKeyEvent *>(ev);
        if (d->wantsOverride(kev)) {
            KEY_DEBUG("OVERRIDING SHORTCUT" << kev->key());
            ev->accept(); // accepting means "don't run the shortcuts"
            return true;
        }
        KEY_DEBUG("NO SHORTCUT OVERRIDE" << kev->key());
        return true;
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
    d->m_fakeEnd = false;
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

#include "fakevimhandler.moc"

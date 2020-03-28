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

#pragma once

#include <QObject>
#include <QTextEdit>

#include <functional>
#include <vector>

namespace FakeVim {
namespace Internal {

enum RangeMode
{
    // Reordering first three enum items here will break
    // compatibility with clipboard format stored by Vim.
    RangeCharMode,         // v
    RangeLineMode,         // V
    RangeBlockMode,        // Ctrl-v
    RangeLineModeExclusive,
    RangeBlockAndTailMode // Ctrl-v for D and X
};

struct Range
{
    Range() = default;
    Range(int b, int e, RangeMode m = RangeCharMode);
    QString toString() const;
    bool isValid() const;

    int beginPos = -1;
    int endPos = -1;
    RangeMode rangemode = RangeCharMode;
};

struct ExCommand
{
    ExCommand() = default;
    ExCommand(const QString &cmd, const QString &args = QString(),
        const Range &range = Range());

    bool matches(const QString &min, const QString &full) const;

    QString cmd;
    bool hasBang = false;
    QString args;
    Range range;
    int count = 1;
};

// message levels sorted by severity
enum MessageLevel
{
    MessageMode,    // show current mode (format "-- %1 --")
    MessageCommand, // show last Ex command or search
    MessageInfo,    // result of a command
    MessageWarning, // warning
    MessageError,   // error
    MessageShowCmd  // partial command
};

template <typename Type>
class Signal final
{
public:
    using Callable = std::function<Type>;

    void connect(const Callable &callable) { m_callables.push_back(callable); }

    template <typename ...Args>
    void operator()(Args ...args) const
    {
        for (const Callable &callable : m_callables)
            callable(args...);
   }

private:
    std::vector<Callable> m_callables;
};

class FakeVimHandler final : public QObject
{
    Q_OBJECT

public:
    explicit FakeVimHandler(QWidget *widget, QObject *parent = nullptr);
    ~FakeVimHandler() override;

    QWidget *widget();

    // call before widget is deleted
    void disconnectFromEditor();

    static void updateGlobalMarksFilenames(const QString &oldFileName, const QString &newFileName);

public:
    void setCurrentFileName(const QString &fileName);
    QString currentFileName() const;

    void showMessage(MessageLevel level, const QString &msg);

    // This executes an "ex" style command taking context
    // information from the current widget.
    void handleCommand(const QString &cmd);
    void handleReplay(const QString &keys);
    void handleInput(const QString &keys);
    void enterCommandMode();

    void installEventFilter();

    // Convenience
    void setupWidget();
    void restoreWidget(int tabSize);

    // Test only
    int physicalIndentation(const QString &line) const;
    int logicalIndentation(const QString &line) const;
    QString tabExpand(int n) const;

    void miniBufferTextEdited(const QString &text, int cursorPos, int anchorPos);

    // Set text cursor position. Keeps anchor if in visual mode.
    void setTextCursorPosition(int position);

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);

    bool jumpToLocalMark(QChar mark, bool backTickMode);

    bool eventFilter(QObject *ob, QEvent *ev) override;

    Signal<void(const QString &msg, int cursorPos, int anchorPos, int messageLevel)> commandBufferChanged;
    Signal<void(const QString &msg)> statusDataChanged;
    Signal<void(const QString &msg)> extraInformationChanged;
    Signal<void(const QList<QTextEdit::ExtraSelection> &selection)> selectionChanged;
    Signal<void(const QString &needle)>  highlightMatches;
    Signal<void(bool *moved, bool *forward, QTextCursor *cursor)> moveToMatchingParenthesis;
    Signal<void(bool *result, QChar c)> checkForElectricCharacter;
    Signal<void(int beginLine, int endLine, QChar typedChar)> indentRegion;
    Signal<void(const QString &needle, bool forward)> simpleCompletionRequested;
    Signal<void(const QString &key, int count)> windowCommandRequested;
    Signal<void(bool reverse)> findRequested;
    Signal<void(bool reverse)> findNextRequested;
    Signal<void(bool *handled, const ExCommand &cmd)> handleExCommandRequested;
    Signal<void()> requestDisableBlockSelection;
    Signal<void(const QTextCursor &cursor)> requestSetBlockSelection;
    Signal<void(QTextCursor *cursor)> requestBlockSelection;
    Signal<void(bool *on)> requestHasBlockSelection;
    Signal<void(int depth)> foldToggle;
    Signal<void(bool fold)> foldAll;
    Signal<void(int depth, bool dofold)> fold;
    Signal<void(int count, bool current)> foldGoTo;
    Signal<void(QChar mark, bool backTickMode, const QString &fileName)> requestJumpToLocalMark;
    Signal<void(QChar mark, bool backTickMode, const QString &fileName)> requestJumpToGlobalMark;
    Signal<void()> completionRequested;
    Signal<void()> tabPreviousRequested;
    Signal<void()> tabNextRequested;

public:
    class Private;

private:
    Private *d;
};

} // namespace Internal
} // namespace FakeVim

Q_DECLARE_METATYPE(FakeVim::Internal::ExCommand)

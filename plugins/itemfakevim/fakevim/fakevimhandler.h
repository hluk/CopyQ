// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "private/fakevim_export.h"

#include <QObject>
#include <QTextEdit>

#include <functional>
#include <vector>

namespace FakeVim::Internal {

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

struct FAKEVIM_EXPORT Range
{
    Range() = default;
    Range(int b, int e, RangeMode m = RangeCharMode);

    bool isValid() const;

    int beginPos = -1;
    int endPos = -1;
    RangeMode rangemode = RangeCharMode;
};

struct FAKEVIM_EXPORT ExCommand
{
    ExCommand() = default;

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

template<typename>
class Callback;

template<typename R, typename... Params>
class Callback<R(Params...)>
{
public:
    static constexpr auto IsVoidReturnType = std::is_same_v<R, void>;
    using Function = std::function<R(Params...)>;
    void set(const Function &callable) { m_callable = callable; }

    R operator()(Params... params)
    {
        if (!m_callable)
            return R();

        if constexpr (IsVoidReturnType)
            m_callable(std::forward<Params>(params)...);
        else
            return m_callable(std::forward<Params>(params)...);
    }

private:
    Function m_callable;
};

class FAKEVIM_EXPORT FakeVimHandler : public QObject
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

    bool inFakeVimMode();

    bool eventFilter(QObject *ob, QEvent *ev) override;

    Callback<void(const QString &msg, int cursorPos, int anchorPos, int messageLevel)>
        commandBufferChanged;
    Callback<void(const QString &msg)> statusDataChanged;
    Callback<void(const QString &msg)> extraInformationChanged;
    Callback<void(const QList<QTextEdit::ExtraSelection> &selection)> selectionChanged;
    Callback<void(const QString &needle)> highlightMatches;
    Callback<void(bool *moved, bool *forward, QTextCursor *cursor)> moveToMatchingParenthesis;
    Callback<void(bool *result, QChar c)> checkForElectricCharacter;
    Callback<void(int beginLine, int endLine, QChar typedChar)> indentRegion;
    Callback<void(const QString &needle, bool forward)> simpleCompletionRequested;
    Callback<void(const QString &key, int count)> windowCommandRequested;
    Callback<void(bool reverse)> findRequested;
    Callback<void(bool reverse)> findNextRequested;
    Callback<void(bool *handled, const ExCommand &cmd)> handleExCommandRequested;
    Callback<void()> requestDisableBlockSelection;
    Callback<void(const QTextCursor &cursor)> requestSetBlockSelection;
    Callback<void(QTextCursor *cursor)> requestBlockSelection;
    Callback<void(bool *on)> requestHasBlockSelection;
    Callback<void(int depth)> foldToggle;
    Callback<void(bool fold)> foldAll;
    Callback<void(int depth, bool dofold)> fold;
    Callback<void(int count, bool current)> foldGoTo;
    Callback<void(QChar mark, bool backTickMode, const QString &fileName)> requestJumpToLocalMark;
    Callback<void(QChar mark, bool backTickMode, const QString &fileName)> requestJumpToGlobalMark;
    Callback<void()> completionRequested;
    Callback<void()> tabPreviousRequested;
    Callback<void()> tabNextRequested;
    Callback<void(bool insertMode)> modeChanged;
    Callback<bool()> tabPressedInInsertMode;
    Callback<void(const QString &, const QString &, QString *)> processOutput;

public:
    class Private;

private:
    Private *d;
};

} // namespace FakeVim::Internal

Q_DECLARE_METATYPE(FakeVim::Internal::ExCommand)

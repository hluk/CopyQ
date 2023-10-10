/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "filterlineedit.h"

#include "common/appconfig.h"
#include "common/compatibility.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/regexp.h"
#include "common/textdata.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/filtercompleter.h"

#include <QKeyEvent>
#include <QMenu>
#include <QModelIndex>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>

namespace {

const QLatin1String optionFilterHistory("filter_history");

class BaseItemFilter : public ItemFilter {
public:
    explicit BaseItemFilter(const QString &searchString)
        : m_searchString(searchString)
    {
    }

    QString searchString() const override
    {
        return m_searchString;
    }

    void highlight(QTextEdit *edit, const QTextCharFormat &format) const override
    {
        QList<QTextEdit::ExtraSelection> selections = this->selections(edit->document(), format);

        // If there are no matches, try to match document without accents/diacritics.
        if (selections.isEmpty()) {
            QTextDocument doc;
            const auto text = edit->document()->toPlainText();
            doc.setPlainText(accentsRemoved(text));
            selections = this->selections(&doc, format);
            for (QTextEdit::ExtraSelection &selection : selections) {
                const auto pos = selection.cursor.position();
                const auto anchor = selection.cursor.anchor();
                selection.cursor = QTextCursor(edit->document());
                selection.cursor.setPosition(pos);
                selection.cursor.setPosition(anchor, QTextCursor::KeepAnchor);
            }
        }

        edit->setExtraSelections(selections);
        edit->update();
    }

    bool matchesAll() const override
    {
        return m_searchString.isEmpty();
    }

    bool matchesIndex(const QModelIndex &index) const override
    {
        // Match formats if the filter expression contains single '/'.
        if ( m_searchString.count('/') != 1 )
            return false;

        const auto re2 = anchoredRegExp(m_searchString);
        const QVariantMap data = index.data(contentType::data).toMap();
        return std::any_of(data.keyBegin(), data.keyEnd(), [&re2](const QString &key) {
            return key.contains(re2);
        });
    }

private:
    virtual QList<QTextEdit::ExtraSelection> selections(QTextDocument *doc, const QTextCharFormat &format) const = 0;

    QString m_searchString;
};

class ItemFilterRegExp final : public BaseItemFilter {
public:
    ItemFilterRegExp(const QRegularExpression &re, const QString &searchString)
        : BaseItemFilter(searchString)
        , m_re(re)
    {
    }

    bool matchesNone() const override
    {
        return !m_re.isValid();
    }

    bool matches(const QString &text) const override
    {
        return text.contains(m_re);
    }

    void search(QTextEdit *edit, bool backwards) const override
    {
        if ( matchesAll() )
            return;

        auto tc = edit->textCursor();
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
            edit->setTextCursor(tc2);
    }

private:
    QList<QTextEdit::ExtraSelection> selections(QTextDocument *doc, const QTextCharFormat &format) const override
    {
        QList<QTextEdit::ExtraSelection> selections;

        if ( m_re.isValid() && !matchesAll() ) {
            QTextCursor cur = doc->find(m_re);
            int a = cur.position();
            while ( !cur.isNull() ) {
                if ( cur.hasSelection() ) {
                    selections.append({cur, format});
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = doc->find(m_re, cur);
                int b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = doc->find(m_re, cur);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }

        return selections;
    }

    QRegularExpression m_re;
};

class ItemFilterFixedStrings final : public BaseItemFilter {
public:
    ItemFilterFixedStrings(const QString &searchString, Qt::CaseSensitivity caseSensitivity)
        : BaseItemFilter(searchString)
        , m_needles( searchString.split(QRegularExpression("\\s+"), SKIP_EMPTY_PARTS) )
        , m_caseSensitivity(caseSensitivity)
    {
    }

    bool matchesNone() const override
    {
        return false;
    }

    bool matches(const QString &text) const override
    {
        return std::all_of(std::begin(m_needles), std::end(m_needles),
            [&text, this](const QString &needle) {
                return text.contains(needle, m_caseSensitivity);
            });
    }

    void search(QTextEdit *edit, bool backwards) const override
    {
        if ( matchesAll() )
            return;

        auto tc = edit->textCursor();
        if ( tc.isNull() )
            return;

        QTextDocument::FindFlags flags;
        if (backwards)
            flags = QTextDocument::FindBackward;
        if (m_caseSensitivity == Qt::CaseSensitive)
            flags = QTextDocument::FindCaseSensitively;

        QTextCursor tc2;
        int minDistance = std::numeric_limits<int>::max();
        for ( const QString &needle : m_needles ) {
            int distance = 0;
            auto tc3 = tc.document()->find(needle, tc, flags);

            // Wrap around.
            if ( tc3.isNull() ) {
                tc3 = tc;
                tc3.movePosition(QTextCursor::End);
                const int endPos = tc3.position();
                if (!backwards)
                    tc3.movePosition(QTextCursor::Start);
                tc3 = tc.document()->find(needle, tc3, flags);
                if ( tc3.isNull() )
                    continue;
                distance = backwards
                    ? tc.selectionStart() + (endPos - tc3.selectionEnd())
                    : endPos - tc.selectionStart() + tc3.selectionStart();
            } else {
                distance = backwards
                    ? tc.selectionEnd() - tc3.selectionEnd()
                    : tc3.selectionStart() - tc.selectionStart();
            }

            // Find longest selection closest to the text cursor.
            if ( tc2.isNull()
                 || distance < minDistance
                 || tc3.selectedText().size() > tc2.selectedText().size() )
            {
                minDistance = distance;
                tc2 = tc3;
            }
        }

        if ( !tc2.isNull() )
            edit->setTextCursor(tc2);
    }

private:
    QList<QTextEdit::ExtraSelection> selections(QTextDocument *doc, const QTextCharFormat &format) const override
    {
        QList<QTextEdit::ExtraSelection> selections;

        QTextDocument::FindFlags flags;
        if (m_caseSensitivity == Qt::CaseSensitive)
            flags = QTextDocument::FindCaseSensitively;

        for ( const QString &needle : m_needles ) {
            QTextCursor cur = doc->find(needle, 0, flags);
            int a = cur.position();
            while ( !cur.isNull() ) {
                if ( cur.hasSelection() ) {
                    selections.append({cur, format});
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = doc->find(needle, cur, flags);
                int b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = doc->find(needle, cur, flags);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }

        return selections;
    }

    QStringList m_needles;
    Qt::CaseSensitivity m_caseSensitivity;
};

class FilterHistory final {
public:
    FilterHistory()
        : m_settings( getConfigurationFilePath("-filter.ini"), QSettings::IniFormat )
    {
    }

    QStringList history() const
    {
        return m_settings.value(optionFilterHistory).toStringList();
    }

    void setHistory(const QStringList &history)
    {
        m_settings.setValue(optionFilterHistory, history);
    }

private:
    QSettings m_settings;
};

// Compatibility with version 2.5.0 and below
void restoreOldFilterHistory()
{
    const QVariant oldHistoryValue = AppConfig().option(optionFilterHistory);
    if (oldHistoryValue.isValid()) {
        const QStringList oldHistory = oldHistoryValue.toStringList();
        if (!oldHistory.isEmpty()) {
            FilterHistory filterHistory;
            QStringList newHistory = filterHistory.history() + oldHistory;
            newHistory.removeDuplicates();
            filterHistory.setHistory(newHistory);
        }
        AppConfig().removeOption(optionFilterHistory);
    }
}

} // namespace

/*!
    The FilterLineEdit class is a fancy line edit customized for
    filtering purposes with a clear button.
*/

namespace Utils {

FilterLineEdit::FilterLineEdit(QWidget *parent)
   : FancyLineEdit(parent)
   , m_timerSearch( new QTimer(this) )
{
    setButtonVisible(Left, true);
    setButtonVisible(Right, true);
    connect(this, &FancyLineEdit::rightButtonClicked, this, &QLineEdit::clear);

    // search timer
    m_timerSearch->setSingleShot(true);
    m_timerSearch->setInterval(200);

    connect( m_timerSearch, &QTimer::timeout,
             this, &FilterLineEdit::filterChanged );
    connect( this, &QLineEdit::textChanged,
             this, &FilterLineEdit::onTextChanged );

    // menu
    auto menu = new QMenu(this);
    setButtonMenu(Left, menu);
    connect( menu, &QMenu::triggered,
             this, &FilterLineEdit::onMenuAction );

    m_actionRe = menu->addAction(tr("Regular Expression"));
    m_actionRe->setCheckable(true);

    m_actionCaseInsensitive = menu->addAction(tr("Case Insensitive"));
    m_actionCaseInsensitive->setCheckable(true);
}

ItemFilterPtr FilterLineEdit::filter() const
{
    const QString pattern = text();
    if (m_actionRe->isChecked()) {
        const auto sensitivity =
            m_actionCaseInsensitive->isChecked()
            ? QRegularExpression::CaseInsensitiveOption
            : QRegularExpression::NoPatternOption;
        return std::make_shared<ItemFilterRegExp>(QRegularExpression(pattern, sensitivity), pattern);
    }

    const auto sensitivity = m_actionCaseInsensitive->isChecked()
        ? Qt::CaseInsensitive : Qt::CaseSensitive;
    return std::make_shared<ItemFilterFixedStrings>(pattern, sensitivity);
}

void FilterLineEdit::loadSettings()
{
    AppConfig appConfig;

    const bool filterRegEx = appConfig.option<Config::filter_regular_expression>();
    m_actionRe->setChecked(filterRegEx);

    const bool filterCaseSensitive = appConfig.option<Config::filter_case_insensitive>();
    m_actionCaseInsensitive->setChecked(filterCaseSensitive);

    const QIcon icon1 = getIcon("edit-clear", IconXmark);
    setButtonIcon(Right, icon1);

    const QIcon icon2 = getIcon("edit-find", IconMagnifyingGlass);
    setButtonIcon(Left, icon2);

    if ( appConfig.option<Config::save_filter_history>() ) {
        if ( !completer() ) {
            FilterCompleter::installCompleter(this);
            restoreOldFilterHistory();
            completer()->setProperty( "history", FilterHistory().history() );
        }
    } else {
        FilterCompleter::removeCompleter(this);
        FilterHistory().setHistory(QStringList());
    }
}

void FilterLineEdit::keyPressEvent(QKeyEvent *ke)
{
    // Up/Down arrow keys should be passed to the item list
    // (on OS X this moves text cursor to the beginning/end).
    if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up) {
        ke->ignore();
        return;
    }

    // If no text is selected, pass copy actions to the item list.
    if ( selectionLength() == 0 && ke->matches(QKeySequence::Copy) ) {
        ke->ignore();
        return;
    }

    FancyLineEdit::keyPressEvent(ke);
}

void FilterLineEdit::hideEvent(QHideEvent *event)
{
    FancyLineEdit::hideEvent(event);

    if (completer()) {
        const QStringList history = completer()->property("history").toStringList();
        FilterHistory().setHistory(history);
    }
}

void FilterLineEdit::focusInEvent(QFocusEvent *event)
{
    FancyLineEdit::focusInEvent(event);
}

void FilterLineEdit::focusOutEvent(QFocusEvent *event)
{
    FancyLineEdit::focusOutEvent(event);

    if ( m_timerSearch->isActive() ) {
        m_timerSearch->stop();
        onTextChanged();
    }
}

void FilterLineEdit::onTextChanged()
{
    if ( hasFocus() )
        m_timerSearch->start();
    else
        emit filterChanged();
}

void FilterLineEdit::onMenuAction()
{
    AppConfig appConfig;
    appConfig.setOption("filter_regular_expression", m_actionRe->isChecked());
    appConfig.setOption("filter_case_insensitive", m_actionCaseInsensitive->isChecked());

    emit filterChanged();
}

} // namespace Utils

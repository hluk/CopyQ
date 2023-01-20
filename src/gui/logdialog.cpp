// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/logdialog.h"
#include "ui_logdialog.h"

#include "common/common.h"
#include "common/log.h"
#include "common/timer.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTimer>

namespace {

const int maxDisplayLogSize = 128 * 1024;
const auto logLinePrefix = "CopyQ ";

void removeLogLines(QByteArray *content, LogLevel level)
{
    const QByteArray label = logLinePrefix + logLevelLabel(level);
    if ( content->startsWith(label) ) {
        const int i = content->indexOf('\n');
        if (i == -1) {
            content->clear();
            return;
        }
        content->remove(0, i);
    }

    const QByteArray labelLine = '\n' + label;
    for (;;) {
        const int i = content->indexOf(labelLine);
        if (i == -1)
            break;

        const int j = content->indexOf('\n', i + 1);
        if (j == -1) {
            content->remove(i, content->size());
            break;
        }

        content->remove(i, j - i);
    }
}

} // namespace

/// Decorates document in batches so it doesn't block UI.
class Decorator : public QObject
{
public:
    Decorator(const QRegularExpression &re, QObject *parent)
        : QObject(parent)
        , m_re(re)
    {
        initSingleShotTimer(&m_timerDecorate, 0, this, &Decorator::decorateBatch);
    }

    /// Start decorating.
    void decorate(QTextDocument *document)
    {
        m_tc = QTextCursor(document);
        m_tc.movePosition(QTextCursor::End);
        decorateBatch();
    }

private:
    void decorateBatch()
    {
        if (m_tc.isNull())
            return;

        QElapsedTimer t;
        t.start();

        do {
            m_tc = m_tc.document()->find(m_re, m_tc, QTextDocument::FindBackward);
            if (m_tc.isNull())
                return;

            decorate(&m_tc);
        } while ( t.elapsed() < 20 );

        m_timerDecorate.start();
    }

    virtual void decorate(QTextCursor *tc) = 0;

    QTimer m_timerDecorate;
    QTextCursor m_tc;
    QRegularExpression m_re;
};

namespace {

class LogDecorator final : public Decorator
{
public:
    LogDecorator(const QFont &font, QObject *parent)
        : Decorator(QRegularExpression("^[^\\]]*\\]"), parent)
        , m_labelNote(logLevelLabel(LogNote))
        , m_labelError(logLevelLabel(LogError))
        , m_labelWarning(logLevelLabel(LogWarning))
        , m_labelDebug(logLevelLabel(LogDebug))
        , m_labelTrace(logLevelLabel(LogTrace))
    {
        QFont boldFont = font;
        boldFont.setBold(true);

        QTextCharFormat normalFormat;
        normalFormat.setFont(boldFont);
        normalFormat.setBackground(Qt::white);
        normalFormat.setForeground(Qt::black);

        m_noteLogLevelFormat = normalFormat;

        m_errorLogLevelFormat = normalFormat;
        m_errorLogLevelFormat.setForeground(Qt::red);

        m_warningLogLevelFormat = normalFormat;
        m_warningLogLevelFormat.setForeground(Qt::darkRed);

        m_debugLogLevelFormat = normalFormat;
        m_debugLogLevelFormat.setForeground(QColor(100, 100, 200));

        m_traceLogLevelFormat = normalFormat;
        m_traceLogLevelFormat.setForeground(QColor(200, 150, 100));
    }

private:
    void decorate(QTextCursor *tc) override
    {
        const QString text = tc->selectedText();
        if ( text.startsWith(m_labelNote) )
            tc->setCharFormat(m_noteLogLevelFormat);
        else if ( text.startsWith(m_labelError) )
            tc->setCharFormat(m_errorLogLevelFormat);
        else if ( text.startsWith(m_labelWarning) )
            tc->setCharFormat(m_warningLogLevelFormat);
        else if ( text.startsWith(m_labelDebug) )
            tc->setCharFormat(m_debugLogLevelFormat);
        else if ( text.startsWith(m_labelTrace) )
            tc->setCharFormat(m_traceLogLevelFormat);
    }

    QString m_labelNote;
    QString m_labelError;
    QString m_labelWarning;
    QString m_labelDebug;
    QString m_labelTrace;

    QTextCharFormat m_noteLogLevelFormat;
    QTextCharFormat m_errorLogLevelFormat;
    QTextCharFormat m_warningLogLevelFormat;
    QTextCharFormat m_debugLogLevelFormat;
    QTextCharFormat m_traceLogLevelFormat;
};

class StringDecorator final : public Decorator
{
public:
    explicit StringDecorator(QObject *parent)
        : Decorator(QRegularExpression("\"[^\"]*\"|'[^']*'"), parent)
    {
        m_stringFormat.setForeground(Qt::darkGreen);
    }

private:
    void decorate(QTextCursor *tc) override
    {
        tc->setCharFormat(m_stringFormat);
    }

    QTextCharFormat m_stringFormat;
};

class ThreadNameDecorator final : public Decorator
{
public:
    explicit ThreadNameDecorator(const QFont &font, QObject *parent)
        : Decorator(QRegularExpression("<[A-Za-z]+-[0-9-]+>"), parent)
    {
        QFont boldFont = font;
        boldFont.setBold(true);
        m_format.setFont(boldFont);
    }

private:
    void decorate(QTextCursor *tc) override
    {
        // Colorize thread label.
        const auto text = tc->selectedText();

        const auto hash = qHash(text);
        const int h = hash % 360;
        m_format.setForeground( QColor::fromHsv(h, 150, 100) );

        const auto bg =
                text.startsWith("<Server-") ? QColor::fromRgb(255, 255, 200)
              : text.startsWith("<monitor") ? QColor::fromRgb(220, 240, 255)
              : text.startsWith("<provide") ? QColor::fromRgb(220, 255, 220)
              : text.startsWith("<synchronize") ? QColor::fromRgb(220, 255, 240)
              : QColor(Qt::white);
        m_format.setBackground(bg);

        tc->setCharFormat(m_format);
    }

    QTextCharFormat m_format;
};

} // namespace

LogDialog::LogDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LogDialog)
    , m_showError(true)
    , m_showWarning(true)
    , m_showNote(true)
    , m_showDebug(true)
    , m_showTrace(true)
{
    ui->setupUi(this);

    auto font = ui->textBrowserLog->font();
    font.setFamily("Monospace");
    ui->textBrowserLog->setFont(font);

    m_logDecorator = new LogDecorator(font, this);
    m_stringDecorator = new StringDecorator(this);
    m_threadNameDecorator = new ThreadNameDecorator(font, this);

    ui->labelLogFileName->setText(logFileName());

    addFilterCheckBox(LogError, &LogDialog::showError);
    addFilterCheckBox(LogWarning, &LogDialog::showWarning);
    addFilterCheckBox(LogNote, &LogDialog::showNote);
    addFilterCheckBox(LogDebug, &LogDialog::showDebug);
    addFilterCheckBox(LogTrace, &LogDialog::showTrace);
    ui->layoutFilters->addStretch(1);

    updateLog();
}

LogDialog::~LogDialog()
{
    delete ui;
}

void LogDialog::updateLog()
{
    QByteArray content = readLogFile(maxDisplayLogSize);

    // Remove first line if incomplete.
    if ( !content.startsWith(logLinePrefix) ) {
        const int i = content.indexOf('\n');
        content.remove(0, i + 1);
    }

    if(!m_showTrace)
        removeLogLines(&content, LogTrace);
    if(!m_showDebug)
        removeLogLines(&content, LogDebug);
    if(!m_showNote)
        removeLogLines(&content, LogNote);
    if(!m_showWarning)
        removeLogLines(&content, LogWarning);
    if(!m_showError)
        removeLogLines(&content, LogError);

    // Remove common prefix.
    const QByteArray prefix = logLinePrefix;
    if ( content.startsWith(prefix) )
        content.remove( 0, prefix.size() );
    content.replace('\n' + prefix, "\n");

    ui->textBrowserLog->setPlainText(QString::fromUtf8(content));

    ui->textBrowserLog->moveCursor(QTextCursor::End);

    QTextDocument *doc = ui->textBrowserLog->document();
    m_logDecorator->decorate(doc);
    m_stringDecorator->decorate(doc);
    m_threadNameDecorator->decorate(doc);
}

void LogDialog::showError(bool show)
{
    m_showError = show;
    updateLog();
}

void LogDialog::showWarning(bool show)
{
    m_showWarning = show;
    updateLog();
}

void LogDialog::showNote(bool show)
{
    m_showNote = show;
    updateLog();
}

void LogDialog::showDebug(bool show)
{
    m_showDebug = show;
    updateLog();
}

void LogDialog::showTrace(bool show)
{
    m_showTrace = show;
    updateLog();
}

void LogDialog::addFilterCheckBox(LogLevel level, FilterCheckBoxSlot slot)
{
    auto checkBox = new QCheckBox(this);
    checkBox->setText(logLevelLabel(level));
    checkBox->setChecked(true);
    QObject::connect(checkBox, &QCheckBox::toggled, this, slot);
    ui->layoutFilters->addWidget(checkBox);
}

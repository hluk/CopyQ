/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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
#include "gui/logdialog.h"
#include "ui_logdialog.h"

#include "common/common.h"
#include "common/log.h"

#include <QElapsedTimer>
#include <QRegExp>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>

namespace {

/// Decorates document in batches so it doesn't block UI.
class Decorator : public QObject
{
    Q_OBJECT
public:
    Decorator(QTextDocument *document, const QRegExp &re, QObject *parent)
        : QObject(parent)
        , m_tc(document)
        , m_re(re)
    {
        m_tc.movePosition(QTextCursor::End);
    }

    /// Start decorating.
    void decorate()
    {
        initSingleShotTimer(&m_timerDecorate, 0, this, SLOT(decorateBatch()));
        decorateBatch();
    }

private slots:
    void decorateBatch()
    {
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

private:
    virtual void decorate(QTextCursor *tc) = 0;

    QTimer m_timerDecorate;
    QTextCursor m_tc;
    QRegExp m_re;
};

} // namespace

class LogDecorator : public Decorator
{
public:
    LogDecorator(QTextDocument *document, QObject *parent)
        : Decorator(document, QRegExp("^.*: "), parent)
        , m_labelNote(logLevelLabel(LogNote))
        , m_labelError(logLevelLabel(LogError))
        , m_labelWarning(logLevelLabel(LogWarning))
        , m_labelDebug(logLevelLabel(LogDebug))
        , m_labelTrace(logLevelLabel(LogTrace))
    {
        QFont boldFont = document->defaultFont();
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
    void decorate(QTextCursor *tc)
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

class StringDecorator : public Decorator
{
public:
    StringDecorator(QTextDocument *document, QObject *parent)
        : Decorator(document, QRegExp("\"[^\"]*\"|'[^']*'"), parent)
    {
        m_stringFormat.setForeground(Qt::darkGreen);
    }

private:
    void decorate(QTextCursor *tc)
    {
        tc->setCharFormat(m_stringFormat);
    }

    QTextCharFormat m_stringFormat;
};

LogDialog::LogDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LogDialog)
{
    ui->setupUi(this);

    QFont font("Monospace");
    ui->textBrowserLog->setFont(font);

    QString content = readLogFile();
    content.replace("\nCopyQ ", "\n");
    ui->textBrowserLog->setPlainText(content);

    ui->textBrowserLog->moveCursor(QTextCursor::End);

    QTextDocument *doc = ui->textBrowserLog->document();

    Decorator *logDecorator = new LogDecorator(doc, this);
    logDecorator->decorate();

    Decorator *stringDecorator = new StringDecorator(doc, this);
    stringDecorator->decorate();
}

LogDialog::~LogDialog()
{
    delete ui;
}

#include "logdialog.moc"

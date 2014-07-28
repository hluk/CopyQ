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

#include "commandsyntaxhighlighter.h"

#include "gui/iconfactory.h"
#include "scriptable/scriptable.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QPalette>
#include <QRegExp>
#include <QSyntaxHighlighter>
#include <QTextEdit>
#include <QPlainTextEdit>

namespace {

QString methodName(const QMetaMethod &method)
{
    QString name =
#if QT_VERSION < 0x050000
            method.signature();
#else
            QString::fromLatin1(method.methodSignature());
#endif

    const int index = name.indexOf('(');

    if (index <= 0)
        return QString();

    return name.remove(index, name.length() - index);
}

QStringList scriptableKeywords()
{
    return QStringList()
            << "break"
            << "do"
            << "instanceof"
            << "typeof"
            << "case"
            << "else"
            << "new"
            << "var"
            << "catch"
            << "finally"
            << "return"
            << "void"
            << "continue"
            << "for"
            << "switch"
            << "while"
            << "debugger"
            << "function"
            << "this"
            << "with"
            << "default"
            << "if"
            << "throw"
            << "delete"
            << "in"
            << "try"
               ;
}

QStringList scriptableFunctions()
{
    QStringList result;

    QMetaObject scriptableMetaObject = Scriptable::staticMetaObject;
    for (int i = 0; i < scriptableMetaObject.methodCount(); ++i) {
        QMetaMethod method = scriptableMetaObject.method(i);

        if (method.methodType() == QMetaMethod::Slot && method.access() == QMetaMethod::Public) {
            const QString name = methodName(method);
            if (name != "deleteLater")
                result.append(name);
        }
    }

    return result;
}

QRegExp createRegExp(const QStringList &list)
{
    QRegExp re;
    re.setPattern("\\b" + list.join("\\b|\\b") + "\\b");
    return re;
}

int mixColorComponent(int a, int b)
{
    return qMin(255, qMax(0, a + b));
}

QColor mixColor(const QColor &color, int r, int g, int b)
{
    return QColor(
                mixColorComponent(color.red(), r),
                mixColorComponent(color.green(), g),
                mixColorComponent(color.blue(), b),
                color.alpha()
                );
}

class CommandSyntaxHighlighter : public QSyntaxHighlighter
{
public:
    explicit CommandSyntaxHighlighter(QWidget *editor, QTextDocument *parent)
        : QSyntaxHighlighter(parent)
        , m_editor(editor)
        , m_reKeywords(createRegExp(scriptableKeywords()))
        , m_reFunctions(createRegExp(scriptableFunctions()))
        , m_reNumbers("(?:\\b|%)\\d+")
    {
    }

protected:
    void highlightBlock(const QString &text)
    {
        const QColor color = getDefaultIconColor(*m_editor, QPalette::Base);

        QTextCharFormat keywordFormat;
        keywordFormat.setFontWeight(QFont::Bold);
        highlight(text, m_reKeywords, keywordFormat);

        QTextCharFormat functionFormat;
        functionFormat.setForeground(mixColor(color, -40, -40, 40));
        highlight(text, m_reFunctions, functionFormat);

        QTextCharFormat stringFormat;
        stringFormat.setForeground(mixColor(color, -40, 40, -40));
        highlightStrings(text, '"', stringFormat);
        highlightStrings(text, '\'', stringFormat);

        QTextCharFormat numberFormat;
        numberFormat.setForeground(mixColor(color, 40, -40, -40));
        highlight(text, m_reNumbers, numberFormat);
    }

private:
    void highlight(const QString &text, QRegExp &re, const QTextCharFormat &format)
    {
        int index = text.indexOf(re);
        while (index >= 0) {
            const int length = re.matchedLength();
            setFormat(index, length, format);
            index = text.indexOf( re, index + qMax(1, length) );
        }
    }

    void highlightStrings(const QString &text, const QChar &quote, const QTextCharFormat &format)
    {
        int start;
        bool inString = false;
        bool escape = false;
        for (int i = 0; i < text.length(); ++i) {
            const QChar c = text[i];
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == quote) {
                if (inString)
                    setFormat(start, i - start + 1, format);
                else
                    start = i;
                inString = !inString;
            }
        }
    }

    QWidget *m_editor;
    QRegExp m_reKeywords;
    QRegExp m_reFunctions;
    QRegExp m_reNumbers;
};

} // namespace

void installCommandSyntaxHighlighter(QTextEdit *editor)
{
    new CommandSyntaxHighlighter(editor, editor->document());
}

void installCommandSyntaxHighlighter(QPlainTextEdit *editor)
{
    new CommandSyntaxHighlighter(editor, editor->document());
}

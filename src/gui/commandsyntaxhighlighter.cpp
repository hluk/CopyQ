/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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
#include <QRegularExpression>
#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptValueIterator>
#include <QSyntaxHighlighter>
#include <QTextEdit>
#include <QPlainTextEdit>

namespace {

QString methodName(const QMetaMethod &method)
{
    auto name = QString::fromLatin1(method.methodSignature());
    const int index = name.indexOf('(');

    if (index <= 0)
        return QString();

    return name.remove(index, name.length() - index);
}

QRegularExpression commandLabelRegExp()
{
    return QRegularExpression(
            "\\bcopyq:"
            "|\\bsh:"
            "|\\bbash:"
            "|\\bpowershell:"
            "|\\bperl:"
            "|\\bpython:"
            "|\\bruby:"
                );
}

QRegularExpression createRegExp(const QStringList &list)
{
    QRegularExpression re;
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

class CommandSyntaxHighlighter final : public QSyntaxHighlighter
{
public:
    explicit CommandSyntaxHighlighter(QWidget *editor, QTextDocument *parent)
        : QSyntaxHighlighter(parent)
        , m_editor(editor)
        , m_reObjects(createRegExp(scriptableObjects()))
        , m_reProperties(createRegExp(scriptableProperties()))
        , m_reFunctions(createRegExp(scriptableFunctions()))
        , m_reKeywords(createRegExp(scriptableKeywords()))
        , m_reLabels(commandLabelRegExp())
        , m_reNumbers("(?:\\b|%)\\d+")
    {
    }

protected:
    void highlightBlock(const QString &text) override
    {
        m_bgColor = getDefaultIconColor(*m_editor);

        QTextCharFormat objectsFormat;
        objectsFormat.setForeground(mixColor(m_bgColor, 40, -60, 40));
        objectsFormat.setToolTip("Object");
        highlight(text, m_reObjects, objectsFormat);

        QTextCharFormat propertyFormat;
        propertyFormat.setForeground(mixColor(m_bgColor, -60, 40, 40));
        highlight(text, m_reProperties, propertyFormat);

        QTextCharFormat functionFormat;
        functionFormat.setForeground(mixColor(m_bgColor, -40, -40, 40));
        highlight(text, m_reFunctions, functionFormat);

        QTextCharFormat keywordFormat;
        keywordFormat.setFontWeight(QFont::Bold);
        highlight(text, m_reKeywords, keywordFormat);

        QTextCharFormat labelsFormat;
        labelsFormat.setFontWeight(QFont::Bold);
        labelsFormat.setForeground(mixColor(m_bgColor, 40, 40, -40));
        highlight(text, m_reLabels, labelsFormat);

        QTextCharFormat numberFormat;
        numberFormat.setForeground(mixColor(m_bgColor, 40, -40, -40));
        highlight(text, m_reNumbers, numberFormat);

        highlightBlocks(text);
    }

private:
    enum State {
        Code,
        SingleQuote,
        DoubleQuote,
        RegExp,
        Comment
    };

    void highlight(const QString &text, QRegularExpression &re, const QTextCharFormat &format)
    {
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), format);
        }
    }

    void format(int a, int b)
    {
        QTextCharFormat format;

        if (currentBlockState() == SingleQuote || currentBlockState() == DoubleQuote) {
            format.setForeground(mixColor(m_bgColor, -40, 40, -40));
        } else if (currentBlockState() == Comment) {
            const int x = m_bgColor.lightness() > 100 ? -40 : 40;
            format.setForeground( mixColor(m_bgColor, x, x, x) );
        } else if (currentBlockState() == RegExp) {
            format.setForeground(mixColor(m_bgColor, 40, -40, -40));
        } else {
            return;
        }

        setFormat(a, b - a + 1, format);
    }

    bool peek(const QString &text, int i, const QString &what)
    {
        return text.midRef(i, what.size()) == what;
    }

    void highlightBlocks(const QString &text)
    {
        bool escape = false;

        setCurrentBlockState(previousBlockState());

        int a = 0;

        for (int i = 0; i < text.size(); ++i) {
            const QChar c = text[i];
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (currentBlockState() == SingleQuote) {
                if (c == '\'') {
                    format(a, i);
                    setCurrentBlockState(Code);
                }
            } else if (currentBlockState() == DoubleQuote) {
                if (c == '"') {
                    format(a, i);
                    setCurrentBlockState(Code);
                }
            } else if (currentBlockState() == Comment) {
                if ( peek(text, i, "*/") ) {
                    ++i;
                    format(a, i);
                    setCurrentBlockState(Code);
                }
            } else if (currentBlockState() == RegExp) {
                if (c == '/') {
                    // Highlight paths outside code as regexps.
                    i = text.indexOf(QRegularExpression("[^a-zA-Z0-9./_-]"), i);
                    if (i == -1)
                        i = text.size();

                    format(a, i);

                    setCurrentBlockState(Code);
                }
            } else if (c == '\\') {
                escape = true;
            } else if ( c == '#' || peek(text, i, "//") ) {
                setCurrentBlockState(Comment);
                format(i, text.size());
                setCurrentBlockState(Code);
                return;
            } else if ( peek(text, i, "/*") ) {
                a = i;
                ++i;
                setCurrentBlockState(Comment);
            } else if (c == '\'') {
                a = i;
                setCurrentBlockState(SingleQuote);
            } else if (c == '"') {
                a = i;
                setCurrentBlockState(DoubleQuote);
            } else if (c == '/') {
                a = i;
                setCurrentBlockState(RegExp);
            }
        }

        format(a, text.size());
    }

    QWidget *m_editor;
    QRegularExpression m_reObjects;
    QRegularExpression m_reProperties;
    QRegularExpression m_reFunctions;
    QRegularExpression m_reKeywords;
    QRegularExpression m_reLabels;
    QRegularExpression m_reNumbers;
    QColor m_bgColor;
};

} // namespace

QStringList scriptableKeywords()
{
    return QStringList()
            << "arguments"
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

QStringList scriptableProperties()
{
    QStringList result;

    QMetaObject scriptableMetaObject = Scriptable::staticMetaObject;
    for (int i = 0; i < scriptableMetaObject.propertyCount(); ++i) {
        QMetaProperty property = scriptableMetaObject.property(i);
        result.append(property.name());
    }

    result.removeOne("objectName");

    return result;
}

QStringList scriptableFunctions()
{
    QStringList result;

    QMetaObject scriptableMetaObject = Scriptable::staticMetaObject;
    for (int i = 0; i < scriptableMetaObject.methodCount(); ++i) {
        QMetaMethod method = scriptableMetaObject.method(i);

        if (method.methodType() == QMetaMethod::Slot && method.access() == QMetaMethod::Public) {
            const QString name = methodName(method);
            result.append(name);
        }
    }

    result.removeOne("deleteLater");

    return result;
}

QStringList scriptableObjects()
{
    QStringList result;
    result.append("ByteArray");
    result.append("Dir");
    result.append("File");
    result.append("TemporaryFile");

    QScriptEngine engine;

    QScriptValue globalObject = engine.globalObject();
    QScriptValueIterator it(globalObject);

    while (it.hasNext()) {
        it.next();
        result.append(it.name());
    }

    return result;
}

void installCommandSyntaxHighlighter(QTextEdit *editor)
{
    new CommandSyntaxHighlighter(editor, editor->document());
}

void installCommandSyntaxHighlighter(QPlainTextEdit *editor)
{
    new CommandSyntaxHighlighter(editor, editor->document());
}

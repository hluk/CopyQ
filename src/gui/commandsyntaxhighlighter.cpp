// SPDX-License-Identifier: GPL-3.0-or-later

#include "commandsyntaxhighlighter.h"

#include "gui/iconfactory.h"
#include "scriptable/scriptable.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QPalette>
#include <QRegularExpression>
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>
#include <QStringList>
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
        , m_reConstants("\\b0x[0-9A-Fa-f]+|(?:\\b|%)\\d+|\\btrue\\b|\\bfalse\\b")
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

        QTextCharFormat constantFormat;
        constantFormat.setForeground(mixColor(m_bgColor, 40, -40, -40));
        highlight(text, m_reConstants, constantFormat);

        highlightBlocks(text);
    }

private:
    enum State {
        Code,
        SingleQuote,
        DoubleQuote,
        BackTick,
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
        const int state = currentBlockState();
        if (state == SingleQuote || state == DoubleQuote || state == BackTick) {
            format.setForeground(mixColor(m_bgColor, -40, 40, -40));
        } else if (state == Comment) {
            const int x = m_bgColor.lightness() > 100 ? -40 : 40;
            format.setForeground( mixColor(m_bgColor, x, x, x) );
        } else if (state == RegExp) {
            format.setForeground(mixColor(m_bgColor, 40, -40, -40));
        } else {
            return;
        }

        setFormat(a, b - a + 1, format);
    }

    bool peek(const QString &text, int i, const QString &what)
    {
        return text.mid(i, what.size()) == what;
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
            } else if (currentBlockState() == BackTick) {
                if (c == '`') {
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
            } else if (c == '`') {
                a = i;
                setCurrentBlockState(BackTick);
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
    QRegularExpression m_reConstants;
    QColor m_bgColor;
};

bool isPublicName(const QString &name)
{
    return !name.startsWith('_');
}

QList<QString> getScriptableObjects()
{
    QJSEngine engine;
    Scriptable scriptable(&engine, nullptr);

    QJSValue globalObject = engine.globalObject();
    QJSValueIterator it(globalObject);

    QList<QString> result;
    while (it.hasNext()) {
        it.next();
        if ( isPublicName(it.name()) )
            result.append(it.name());
    }

    return result;
}

} // namespace

QList<QString> scriptableKeywords()
{
    return {
        QStringLiteral("arguments"),
        QStringLiteral("break"),
        QStringLiteral("case"),
        QStringLiteral("catch"),
        QStringLiteral("const"),
        QStringLiteral("continue"),
        QStringLiteral("debugger"),
        QStringLiteral("default"),
        QStringLiteral("delete"),
        QStringLiteral("do"),
        QStringLiteral("else"),
        QStringLiteral("finally"),
        QStringLiteral("for"),
        QStringLiteral("function"),
        QStringLiteral("if"),
        QStringLiteral("in"),
        QStringLiteral("instanceof"),
        QStringLiteral("let"),
        QStringLiteral("new"),
        QStringLiteral("of"),
        QStringLiteral("return"),
        QStringLiteral("switch"),
        QStringLiteral("this"),
        QStringLiteral("throw"),
        QStringLiteral("try"),
        QStringLiteral("typeof"),
        QStringLiteral("var"),
        QStringLiteral("void"),
        QStringLiteral("while"),
        QStringLiteral("with"),
    };
}

QList<QString> scriptableProperties()
{
    QList<QString> result;

    QMetaObject scriptableMetaObject = Scriptable::staticMetaObject;
    for (int i = 0; i < scriptableMetaObject.propertyCount(); ++i) {
        QMetaProperty property = scriptableMetaObject.property(i);
        if ( isPublicName(property.name()) )
            result.append(property.name());
    }

    result.removeOne("objectName");

    return result;
}

QList<QString> scriptableFunctions()
{
    QList<QString> result;

    QMetaObject scriptableMetaObject = Scriptable::staticMetaObject;
    for (int i = 0; i < scriptableMetaObject.methodCount(); ++i) {
        QMetaMethod method = scriptableMetaObject.method(i);

        if (method.methodType() == QMetaMethod::Slot && method.access() == QMetaMethod::Public) {
            const QString name = methodName(method);
            if ( isPublicName(name) )
                result.append(name);
        }
    }

    result.removeOne("deleteLater");

    return result;
}

QList<QString> scriptableObjects()
{
    static const QList<QString> result = getScriptableObjects();
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

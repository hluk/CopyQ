#ifndef REGEXP_H
#define REGEXP_H

#include <QRegularExpression>

static QRegularExpression anchoredRegExp(const QString &pattern)
{
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
}

#endif // REGEXP_H

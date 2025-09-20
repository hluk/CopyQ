#pragma once


#include <QRegularExpression>

static QRegularExpression anchoredRegExp(const QString &pattern)
{
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
}

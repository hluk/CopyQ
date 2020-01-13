#ifndef REGEXP_H
#define REGEXP_H

#include <QRegularExpression>

static QRegularExpression anchoredRegExp(const QString &pattern)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,12,0)
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
#else
    const auto anchoredPattern = QLatin1String("\\A(?:") + pattern + QLatin1String(")\\z");
    return QRegularExpression(anchoredPattern);
#endif
}

#endif // REGEXP_H

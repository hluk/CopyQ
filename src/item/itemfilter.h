#pragma once

#include <memory>

class QModelIndex;
class QString;
class QTextCharFormat;
class QTextEdit;

class ItemFilter
{
public:
    virtual ~ItemFilter() = default;
    virtual bool matchesAll() const = 0;
    virtual bool matchesNone() const = 0;
    virtual bool matches(const QString &text) const = 0;
    virtual bool matchesIndex(const QModelIndex &index) const = 0;
    virtual void highlight(QTextEdit *edit, const QTextCharFormat &format) const = 0;
    virtual void search(QTextEdit *edit, bool backwards) const = 0;
    virtual QString searchString() const = 0;
};

using ItemFilterPtr = std::shared_ptr<ItemFilter>;

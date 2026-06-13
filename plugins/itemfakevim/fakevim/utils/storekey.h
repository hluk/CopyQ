// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QByteArray>
#include <QString>
#include <QHashFunctions>

namespace Utils {

class Key
{
public:
    Key() = default;
    Key(const QByteArray &key) : data(key) {}
    template <int N> Key(const char (&key)[N]) : data(key) {}
    const QByteArray &toByteArray() const { return data; }
    bool isEmpty() const { return data.isEmpty(); }
    friend bool operator<(const Key &a, const Key &b) { return a.data < b.data; }
    friend bool operator==(const Key &a, const Key &b) { return a.data == b.data; }
    friend Key operator+(const Key &a, const Key &b) { return Key(a.data + b.data); }
    friend size_t qHash(const Key &key, size_t seed = 0) { return qHash(key.data, seed); }
private:
    QByteArray data;
};

inline Key keyFromString(const QString &str) { return Key(str.toUtf8()); }
inline QString stringFromKey(const Key &key) { return QString::fromUtf8(key.toByteArray()); }

} // namespace Utils

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJSValue>
#include <QObject>

class ScriptableNetworkReply;

class ScriptableNetworkRequest final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QJSValue headers READ headers WRITE setHeaders)
    Q_PROPERTY(QJSValue maxRedirects READ maxRedirects WRITE setMaxRedirects)

public:
    Q_INVOKABLE ScriptableNetworkRequest() = default;

    QJSValue headers();
    void setHeaders(const QJSValue &headers) { m_headers = headers; }

    QJSValue maxRedirects() const { return m_maxRedirects; }
    void setMaxRedirects(const QJSValue &redirect) { m_maxRedirects = redirect; }

    ScriptableNetworkReply *requestRaw(
        const QByteArray &method, const QString &url, const QByteArray &data) const;

public slots:
    QJSValue request(
        const QJSValue &method, const QJSValue &url, const QJSValue &data = {}) const;

private:
    QJSEngine *engine() const;
    QJSValue newByteArray(const QByteArray &bytes) const;

    QJSValue m_headers;
    QJSValue m_maxRedirects = 0;
};

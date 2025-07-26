// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptablenetworkrequest.h"
#include "scriptablenetworkreply.h"

#include "scriptable/scriptablebytearray.h"
#include "common/log.h"
#include "common/version.h"

#include <QJSEngine>
#include <QJSValueIterator>
#include <QNetworkAccessManager>
#include <QNetworkRequest>

Q_DECLARE_METATYPE(QByteArray*)

ScriptableNetworkReply *ScriptableNetworkRequest::requestRaw(const QByteArray &method, const QString &url, const QByteArray &data) const
{
    QNetworkRequest request;

    const int maxRedirects = m_maxRedirects.toInt();
    request.setUrl(QUrl(url));
    request.setMaximumRedirectsAllowed(maxRedirects);

    QJSValueIterator it(m_headers);
    while (it.hasNext()) {
        it.next();
        const QByteArray name = it.name().toUtf8();
        const QByteArray value = toByteArray(it.value());
        request.setRawHeader(name, value);
    }

    auto reply = new ScriptableNetworkReply();
    auto manager = new QNetworkAccessManager(reply);
    manager->setRedirectPolicy(
        maxRedirects > 0
        ? QNetworkRequest::NoLessSafeRedirectPolicy
        : QNetworkRequest::ManualRedirectPolicy);
    COPYQ_LOG(
        QStringLiteral(
            "ScriptableNetworkRequest: method=%1, url=%2, data=%3, maxRedirects=%4")
        .arg(QString::fromUtf8(method), url, QString::fromUtf8(data), QString::number(maxRedirects)));
    QNetworkReply *managerReply = data.isNull()
        ? manager->sendCustomRequest(request, method)
        : manager->sendCustomRequest(request, method, data);
    reply->setReply(managerReply);
    return reply;
}

QJSValue ScriptableNetworkRequest::headers()
{
    if (m_headers.isUndefined() || m_headers.isNull()) {
        m_headers = engine()->newObject();
        m_headers.setProperty(
            QStringLiteral("User-Agent"),
            QStringLiteral("CopyQ/%1").arg(versionString));
    }
    return m_headers;
}

QJSValue ScriptableNetworkRequest::request(const QJSValue &method, const QJSValue &url, const QJSValue &data) const
{
    const QByteArray postData = (data.isUndefined() || data.isNull())
        ? QByteArray()
        : toByteArray(data);
    auto reply = requestRaw(toByteArray(method), toString(url), postData);
    return engine()->newQObject(reply);
}

QJSEngine *ScriptableNetworkRequest::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableNetworkRequest::newByteArray(const QByteArray &bytes) const
{
    return engine()->newQObject( new ScriptableByteArray(bytes) );
}

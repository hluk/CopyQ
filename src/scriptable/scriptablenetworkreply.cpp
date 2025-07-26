// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptablenetworkreply.h"

#include "scriptable/scriptablebytearray.h"

#include <QJSEngine>
#include <QEventLoop>
#include <QNetworkReply>

ScriptableNetworkReply::~ScriptableNetworkReply()
{
    if (m_reply)
        m_reply->deleteLater();
}

QJSValue ScriptableNetworkReply::data()
{
    if (!m_reply)
        return {};

    if ( !m_data.isUndefined() )
        return m_data;

    if ( !m_reply->isFinished() ) {
        QEventLoop loop;
        connect(m_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    if ( !m_reply->isFinished() )
        return QJSValue();

    m_data = newByteArray(m_rawData);

    return m_data;
}

QJSValue ScriptableNetworkReply::error()
{
    if (!m_reply)
        return {};

    data();

    if (m_reply->error() != QNetworkReply::NoError)
        return m_reply->errorString();

    return QJSValue();
}

QJSValue ScriptableNetworkReply::status()
{
    if (!m_reply)
        return {};

    data();

    const QVariant v = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (v.isValid())
        return v.toInt();
    return QJSValue();
}

QJSValue ScriptableNetworkReply::redirect()
{
    if (!m_reply)
        return {};

    data();

    const QVariant v = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (v.isValid())
        return v.toUrl().resolved(m_reply->url()).toString();
    return QJSValue();
}

QJSValue ScriptableNetworkReply::headers()
{
    if (!m_reply)
        return {};

    data();

    QJSValue headers = engine()->newArray();
    int i = 0;
    for ( const auto &header : m_reply->rawHeaderList() ) {
        QJSValue pair = engine()->newArray();
        pair.setProperty( 0, newByteArray(header) );
        pair.setProperty( 1, newByteArray(m_reply->rawHeader(header)) );
        headers.setProperty( static_cast<quint32>(i), pair );
        ++i;
    }

    return headers;
}

QJSValue ScriptableNetworkReply::finished()
{
    return !m_reply || m_reply->isFinished();
}

QJSValue ScriptableNetworkReply::url()
{
    if (!m_reply)
        return {};

    data();

    return m_reply->url().toString();
}

void ScriptableNetworkReply::setReply(QNetworkReply *reply)
{
    m_reply = reply;

    connect(m_reply, &QNetworkReply::readyRead, this, [this](){
        const qint64 available = m_reply->bytesAvailable();
        m_rawData.append( m_reply->read(available) );
    });
    const qint64 available = m_reply->bytesAvailable();
    m_rawData.append( m_reply->read(available) );
}

QJSEngine *ScriptableNetworkReply::engine() const
{
    return qjsEngine(this);
}

QJSValue ScriptableNetworkReply::newByteArray(const QByteArray &bytes) const
{
    return engine()->newQObject( new ScriptableByteArray(bytes) );
}

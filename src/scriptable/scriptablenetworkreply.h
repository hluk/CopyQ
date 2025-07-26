// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJSValue>
#include <QObject>

class QNetworkReply;

class ScriptableNetworkReply final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QJSValue data READ data CONSTANT)
    Q_PROPERTY(QJSValue error READ error CONSTANT)
    Q_PROPERTY(QJSValue status READ status CONSTANT)
    Q_PROPERTY(QJSValue redirect READ redirect CONSTANT)
    Q_PROPERTY(QJSValue headers READ headers CONSTANT)
    Q_PROPERTY(QJSValue finished READ finished CONSTANT)
    Q_PROPERTY(QJSValue url READ url CONSTANT)

public:
    Q_INVOKABLE ScriptableNetworkReply() = default;

    ~ScriptableNetworkReply();

    QJSValue data();

    QJSValue error();

    QJSValue status();
    QJSValue redirect();
    QJSValue headers();

    QJSValue finished();

    QJSValue url();

    void setReply(QNetworkReply *reply);

private:
    QJSEngine *engine() const;
    QJSValue newByteArray(const QByteArray &bytes) const;

    QNetworkReply *m_reply = nullptr;
    QJSValue m_data;
    QJSValue m_self;
    QByteArray m_rawData;
};

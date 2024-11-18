// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QtContainerFwd>
#include <QMetaType>

class QMimeData;
class QImage;
class QUrl;

// Avoids accessing old clipboard/drag'n'drop data.
class ClipboardDataGuard final {
public:
    explicit ClipboardDataGuard(
        const QMimeData *data,
        const long int *clipboardSequenceNumber = nullptr);

    ~ClipboardDataGuard();

    QStringList formats();
    bool hasFormat(const QString &mime);

    QByteArray data(const QString &mime);

    QList<QUrl> urls();
    QString text();
    bool hasText();
    QImage getImageData();
    QByteArray getUtf8Data(const QString &format);
    bool isExpired();
    const QMimeData *mimeData();

private:
    const QMimeData *m_data;
    QElapsedTimer m_timerExpire;
    const long int *m_clipboardSequenceNumber;
    long int m_clipboardSequenceNumberOriginal;
    QMetaObject::Connection m_connection;
};

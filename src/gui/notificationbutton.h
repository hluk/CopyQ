// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>

struct NotificationButton
{
    QString name;
    QString script;
    QByteArray data;
};

Q_DECLARE_METATYPE(NotificationButton)

using NotificationButtons = QList<NotificationButton>;

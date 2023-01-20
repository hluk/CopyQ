// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOTIFICATIONBUTTON_H
#define NOTIFICATIONBUTTON_H

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

#endif // NOTIFICATIONBUTTON_H

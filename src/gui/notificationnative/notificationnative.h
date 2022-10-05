// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class Notification;
class QColor;
class QObject;

Notification *createNotificationNative(const QColor &iconColor, QObject *parent);

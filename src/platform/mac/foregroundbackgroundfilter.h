// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FOREGROUNDBACKGROUNDFILTER_H
#define FOREGROUNDBACKGROUNDFILTER_H

#include <QObject>

/**
 * This event filter manages the "activationPolicy" for an OS X app by
 * ensuring that it is a "regular" app when there are windows shown, but
 * an "accessory" or "prohibited"/"background" app when there are none.
 *
 * This allows the app to not have a dock icon unless there is an open window.
 *
 * If only menu or some notifications are visible dock icon is hidden.
 */
class ForegroundBackgroundFilter final : public QObject
{
public:
    /**
     * Install the filter to parent.
     */
    static void installFilter(QObject *parent);

protected:
    bool eventFilter(QObject *obj, QEvent *ev);
    ForegroundBackgroundFilter(QObject *parent);
};

#endif // FOREGROUNDBACKGROUNDFILTER_H

// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACTIONOUTPUT_H
#define ACTIONOUTPUT_H

class Action;
class MainWindow;
class QString;
class QRegularExpression;
class QModelIndex;

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QString &outputTabName,
        const QRegularExpression &itemSeparator
        );

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QString &outputTabName
        );

void actionOutput(
        MainWindow *wnd,
        Action *action,
        const QString &outputItemFormat,
        const QModelIndex &index
        );

#endif // ACTIONOUTPUT_H

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


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

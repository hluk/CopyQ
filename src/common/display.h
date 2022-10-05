// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DISPLAY_H
#define DISPLAY_H

class QPoint;
class QWidget;

int smallIconSize();

QPoint toScreen(QPoint pos, QWidget *w);

int pointsToPixels(int points, QWidget *w = nullptr);

#endif // DISPLAY_H

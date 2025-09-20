// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


class QPoint;
class QWidget;

int smallIconSize();

QPoint toScreen(QPoint pos, QWidget *w);

int pointsToPixels(int points, QWidget *w = nullptr);

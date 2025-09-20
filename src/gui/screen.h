#pragma once


class QPoint;
class QRect;
class QWidget;

int screenCount();

int screenNumberAt(const QPoint &pos);

QRect screenGeometry(int i);

QRect screenAvailableGeometry(const QWidget &w);

#ifndef SCREEN_H
#define SCREEN_H

class QPoint;
class QRect;

int screenCount();

int screenNumberAt(const QPoint &pos);

QRect screenGeometry(int i);

QRect screenAvailableGeometry(const QPoint &pos);

#endif // SCREEN_H

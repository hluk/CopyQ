// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WINDOWGEOMETRYGUARD_H
#define WINDOWGEOMETRYGUARD_H

#include <QObject>
#include <QWidget>
#include <QTimer>

class WindowGeometryGuard final : public QObject
{
public:
    static void create(QWidget *window);

    bool eventFilter(QObject *object, QEvent *event) override;

private:
    explicit WindowGeometryGuard(QWidget *window);

    bool isWindowGeometryLocked() const;

    bool lockWindowGeometry();

    void saveWindowGeometry();
    void restoreWindowGeometry();
    void unlockWindowGeometry();

    void onScreenChanged();

    QWidget *m_window;

    QTimer m_timerSaveGeometry;
    QTimer m_timerRestoreGeometry;
    QTimer m_timerUnlockGeometry;
};

#endif // WINDOWGEOMETRYGUARD_H

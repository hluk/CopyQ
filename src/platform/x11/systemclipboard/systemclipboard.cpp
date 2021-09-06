/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "systemclipboard.h"

#include "waylandclipboard.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>

SystemClipboard *SystemClipboard::instance()
{
    if (!qApp || qApp->closingDown()) {
        return nullptr;
    }
    static WaylandClipboard *systemClipboard = nullptr;
    if (!systemClipboard) {
        qInfo() << "Using Wayland clipboard access";
        systemClipboard = new WaylandClipboard(qApp);

        QElapsedTimer timer;
        timer.start();
        while ( !systemClipboard->isActive() && timer.elapsed() < 5000 ) {
            QCoreApplication::processEvents();
        }
        if ( timer.elapsed() > 100 ) {
            qWarning() << "Activating Wayland clipboard took" << timer.elapsed() << "ms";
        }
        if ( !systemClipboard->isActive() ) {
            qCritical() << "Failed to activate Wayland clipboard";
        }
    }
    return systemClipboard;
}

SystemClipboard::SystemClipboard(QObject *parent)
    : QObject(parent)
{
}


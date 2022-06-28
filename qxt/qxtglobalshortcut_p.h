#ifndef QXTGLOBALSHORTCUT_P_H
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#define QXTGLOBALSHORTCUT_P_H

#include <QAbstractEventDispatcher>
#include <QHash>

class QKeySequence;
class QxtGlobalShortcut;

#include <QAbstractNativeEventFilter>

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
using NativeEventResult = qintptr;
#else
using NativeEventResult = long;
#endif

class QxtGlobalShortcutPrivate
    : public QAbstractNativeEventFilter
{
public:
    explicit QxtGlobalShortcutPrivate(QxtGlobalShortcut *q);
    ~QxtGlobalShortcutPrivate();

    bool enabled;
    Qt::Key key;
    Qt::KeyboardModifiers mods;
    quint32 nativeKey;
    quint32 nativeMods;
    bool registered;

    bool setShortcut(const QKeySequence& shortcut);
    bool unsetShortcut();

#   ifndef Q_OS_MAC
    static int ref;
#   endif // Q_OS_MAC

    bool nativeEventFilter(
        const QByteArray &eventType, void *message, NativeEventResult *result) override;

    static void activateShortcut(quint32 nativeKey, quint32 nativeMods);

private:
    QxtGlobalShortcut *q_ptr;

    static quint32 nativeKeycode(Qt::Key keycode, Qt::KeyboardModifiers mods);
    static quint32 nativeModifiers(Qt::KeyboardModifiers modifiers);

    static bool registerShortcut(quint32 nativeKey, quint32 nativeMods);
    static bool unregisterShortcut(quint32 nativeKey, quint32 nativeMods);

    static QHash<QPair<quint32, quint32>, QxtGlobalShortcut*> shortcuts;
};

#endif // QXTGLOBALSHORTCUT_P_H

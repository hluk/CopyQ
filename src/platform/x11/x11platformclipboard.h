/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef X11PLATFORMCLIPBOARD_H
#define X11PLATFORMCLIPBOARD_H

#include "platform/dummy/dummyclipboard.h"

#include <QClipboard>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>

class X11DisplayGuard;

class X11PlatformClipboard : public DummyClipboard
{
    Q_OBJECT
public:
    X11PlatformClipboard(const QSharedPointer<X11DisplayGuard> &d);

    void loadSettings(const QVariantMap &settings);

    QVariantMap data(const QStringList &formats) const;

    void ignoreCurrentData();

private slots:
    void onChanged(QClipboard::Mode mode);
    void checkSelectionComplete();
    void resetClipboard();
    void synchronize();

private:
    void initSingleShotTimer(int intervalMs, const char *slot, QTimer *timer);

    bool waitIfSelectionIncomplete();

    /**
     * Remember last non-empty clipboard content and reset clipboard after interval if there is no owner.
     *
     * @return return true if clipboard/selection has no owner and will be reset
     */
    bool maybeResetClipboard(QClipboard::Mode mode);

    void syncFrom(QClipboard::Mode mode);

    QSharedPointer<X11DisplayGuard> d;

    bool m_copyclip;
    bool m_checksel;
    bool m_copysel;
    QStringList m_formats;

    bool m_lastChangedIsClipboard;
    bool m_resetClipboard;
    bool m_resetSelection;
    bool m_syncFromClipboard;

    QTimer m_timerIncompleteSelection;
    QTimer m_timerReset;
    QTimer m_timerSync;

    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;
};

#endif // X11PLATFORMCLIPBOARD_H

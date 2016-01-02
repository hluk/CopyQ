/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

    QVariantMap data(Mode mode, const QStringList &formats) const;

    void setData(Mode mode, const QVariantMap &dataMap);

private slots:
    void onChanged(QClipboard::Mode mode);
    void checkSelectionComplete();
    void resetClipboard();

private:
    bool waitIfSelectionIncomplete();

    /**
     * Remember last non-empty clipboard content and reset clipboard after interval if there is no owner.
     *
     * @return return true if clipboard/selection has no owner and will be reset
     */
    bool maybeResetClipboard(QClipboard::Mode mode);

    QSharedPointer<X11DisplayGuard> d;

    QStringList m_formats;

    bool m_resetClipboard;
    bool m_resetSelection;

    QTimer m_timerIncompleteSelection;
    QTimer m_timerReset;

    QVariantMap m_clipboardData;
    QVariantMap m_selectionData;
};

#endif // X11PLATFORMCLIPBOARD_H

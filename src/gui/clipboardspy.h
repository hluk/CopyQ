/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDSPY_H
#define CLIPBOARDSPY_H

#include "common/clipboardmode.h"

#include <QObject>

class ClipboardSpy final : public QObject
{
    Q_OBJECT
public:
    explicit ClipboardSpy(ClipboardMode mode);

    /// Actively wait for clipboard/selection to change.
    void wait(int ms = 2000);

signals:
    void changed();

private:
    void onChanged();
    bool check();

    ClipboardMode m_mode;
    bool m_changed = false;
    QByteArray m_oldOwnerData;
};

#endif // CLIPBOARDSPY_H

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

#ifndef DUMMYCLIPBOARD_H
#define DUMMYCLIPBOARD_H

#include "platform/platformclipboard.h"

#include <QClipboard>

class DummyClipboard : public PlatformClipboard
{
    Q_OBJECT
public:
    explicit DummyClipboard(bool connectClipboardSignal = true);

    void loadSettings(const QVariantMap &) {}

    QVariantMap data(const QStringList &formats) const;

    void setData(const QVariantMap &dataMap);

    void ignoreCurrentData() {}

signals:
    void changed();

protected:
    QVariantMap data(QClipboard::Mode mode, const QStringList &formats) const;
    void setData(QClipboard::Mode mode, const QVariantMap &dataMap);

private slots:
    virtual void onChanged(QClipboard::Mode mode);
};

#endif // DUMMYCLIPBOARD_H

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

#ifndef ICONSELECTBUTTON_H
#define ICONSELECTBUTTON_H

#include <QPushButton>

class IconSelectButton final : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString currentIcon READ currentIcon WRITE setCurrentIcon NOTIFY currentIconChanged)
public:
    explicit IconSelectButton(QWidget *parent = nullptr);

    const QString &currentIcon() const { return m_currentIcon; }

    QSize sizeHint() const override;

    void setCurrentIcon(const QString &iconString);

signals:
    void currentIconChanged(const QString &icon);

private:
    void onClicked();

    QString m_currentIcon;
};

#endif // ICONSELECTBUTTON_H

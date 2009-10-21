/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

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

#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QLineEdit>

class SearchBar : public QLineEdit
{
    Q_OBJECT
    public:
        SearchBar(QWidget *parent = 0);
};

#endif // SEARCHBAR_H

/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#ifndef ITEMRICHTEXT_H
#define ITEMRICHTEXT_H

#include "itemwidget.h"

#include <QTextEdit>

class QTextDocument;

class ItemRichText : public QTextEdit, public ItemWidget
{
    Q_OBJECT
public:
    ItemRichText(const QString &html, QWidget *parent);

    QWidget *widget() { return this; }

protected:
    void highlight(const QRegExp &re, const QFont &highlightFont,
                      const QPalette &highlightPalette);

private:
    QTextDocument *m_doc;
};

#endif // ITEMRICHTEXT_H

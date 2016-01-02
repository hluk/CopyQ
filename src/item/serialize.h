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

#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <QVariantMap>

class QAbstractItemModel;
class QByteArray;
class QDataStream;
class QFile;

void serializeData(QDataStream *out, const QVariantMap &data);
void deserializeData(QDataStream *stream, QVariantMap *data);
QByteArray serializeData(const QVariantMap &data);
bool deserializeData(QVariantMap *data, const QByteArray &bytes);

bool serializeData(const QAbstractItemModel &model, QDataStream *stream);
bool deserializeData(QAbstractItemModel *model, QDataStream *stream);
bool serializeData(const QAbstractItemModel &model, QFile *file);
bool deserializeData(QAbstractItemModel *model, QFile *file);

#endif // SERIALIZE_H

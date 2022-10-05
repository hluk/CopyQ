// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <QVariantMap>

class QAbstractItemModel;
class QByteArray;
class QDataStream;
class QIODevice;

void serializeData(QDataStream *stream, const QVariantMap &data);
bool deserializeData(QDataStream *stream, QVariantMap *data);
QByteArray serializeData(const QVariantMap &data);
bool deserializeData(QVariantMap *data, const QByteArray &bytes);

bool serializeData(const QAbstractItemModel &model, QDataStream *stream);
bool deserializeData(QAbstractItemModel *model, QDataStream *stream, int maxItems);
bool serializeData(const QAbstractItemModel &model, QIODevice *file);
bool deserializeData(QAbstractItemModel *model, QIODevice *file, int maxItems);

#endif // SERIALIZE_H

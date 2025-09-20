// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QtContainerFwd>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QVariantMap>
#endif

class QAbstractItemModel;
class QByteArray;
class QDataStream;
class QIODevice;

class DataFile;
QDataStream &operator<<(QDataStream &out, DataFile value);
QDataStream &operator>>(QDataStream &in, DataFile &value);
void registerDataFileConverter();

void serializeData(QDataStream *stream, const QVariantMap &data, int itemDataThreshold = -1);
bool deserializeData(QDataStream *stream, QVariantMap *data);
QByteArray serializeData(const QVariantMap &data);
bool deserializeData(QVariantMap *data, const QByteArray &bytes);

bool serializeData(const QAbstractItemModel &model, QDataStream *stream, int itemDataThreshold = -1);
bool deserializeData(QAbstractItemModel *model, QDataStream *stream);
bool serializeData(const QAbstractItemModel &model, QIODevice *file, int itemDataThreshold = -1);
bool deserializeData(QAbstractItemModel *model, QIODevice *file);

QString itemDataPath();
bool itemDataFiles(QIODevice *file, QStringList *files);

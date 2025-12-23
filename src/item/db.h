// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QSqlDatabase;
class QString;

int getOrCreateTabId(QSqlDatabase &db, const QString &tabName);

QSqlDatabase openDb();

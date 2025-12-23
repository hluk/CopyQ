// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QSqlDatabase;
class QString;

int getOrCreateTabId(QSqlDatabase &db, const QString &tabName);

QSqlDatabase openDb(const QString &password, const QString &dbName);

bool setDbPassword(QSqlDatabase &db, const QString &password);

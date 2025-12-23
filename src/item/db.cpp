// SPDX-License-Identifier: GPL-3.0-or-later

#include "db.h"
#include "common/config.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

namespace {

Q_DECLARE_LOGGING_CATEGORY(logCategory)
Q_LOGGING_CATEGORY(logCategory, "copyq.db")

const QString dbType = QStringLiteral("QSQLITE");
const QString dbUser = QStringLiteral("copyq");

namespace query {

const QString schemaExists = QStringLiteral(
    "SELECT version FROM schema_version WHERE version = 1");
const QString createVersionTable = QStringLiteral(
    "CREATE TABLE IF NOT EXISTS schema_version ("
    "version INTEGER PRIMARY KEY, "
    "applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
const QString createTabsTable = QStringLiteral(
    "CREATE TABLE IF NOT EXISTS tabs ("
    "tab_id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "tab_name TEXT NOT NULL UNIQUE, "
    "icon_name TEXT, "
    "max_items INTEGER, "
    "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
    "modified_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
const QString createItemsTable = QStringLiteral(
    "CREATE TABLE IF NOT EXISTS items ("
    "item_id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "tab_id INTEGER NOT NULL, "
    "row_index INTEGER NOT NULL, "
    "item_hash INTEGER NOT NULL, "
    "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
    "FOREIGN KEY (tab_id) REFERENCES tabs(tab_id) ON DELETE CASCADE, "
    "UNIQUE(tab_id, row_index))");
const QString createItemDataTable = QStringLiteral(
    "CREATE TABLE IF NOT EXISTS item_data ("
    "data_id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "item_id INTEGER NOT NULL, "
    "format TEXT NOT NULL, "
    "bytes BLOB, "
    "FOREIGN KEY (item_id) REFERENCES items(item_id) ON DELETE CASCADE, "
    "UNIQUE(item_id, format))");
const QString createIndexItemsTabIdRow = QStringLiteral(
    "CREATE INDEX IF NOT EXISTS idx_items_tab_row ON items(tab_id, row_index)");
const QString createIndexItemsHash = QStringLiteral(
    "CREATE INDEX IF NOT EXISTS idx_items_hash ON items(item_hash)");
const QString createIndexItemDataItemId = QStringLiteral(
    "CREATE INDEX IF NOT EXISTS idx_item_data_item ON item_data(item_id)");
const QString markVersion1 = QStringLiteral(
    "INSERT INTO schema_version (version) VALUES (1)");
const QString selectTab = QStringLiteral(
    "SELECT tab_id FROM tabs WHERE tab_name = ?");
const QString createTab = QStringLiteral(
    "INSERT INTO tabs (tab_name) VALUES (?)");

} // namespace query

bool initializeSchema(QSqlDatabase &db)
{
    QSqlQuery query(db);

    if (query.exec(query::schemaExists) && query.next()) {
        return true;
    }

    const auto queries = {
        query::createVersionTable,
        query::createVersionTable,
        query::createTabsTable,
        query::createItemsTable,
        query::createItemDataTable,
        query::createIndexItemsTabIdRow,
        query::createIndexItemsHash,
        query::createIndexItemDataItemId,
        query::markVersion1,
    };

    for (const auto &sql : queries) {
        if (!query.exec(sql)) {
            qCCritical(logCategory) << "Failed to execute query:" << sql << "; error:" << query.lastError().text();
            return false;
        }
    }

    qCInfo(logCategory) << "Database schema initialized successfully";
    return true;
}

} // namespace

int getOrCreateTabId(QSqlDatabase &db, const QString &tabName)
{
    QSqlQuery query(db);

    query.prepare(query::selectTab);
    query.addBindValue(tabName);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    query.prepare(query::createTab);
    query.addBindValue(tabName);
    if (!query.exec()) {
        qWarning() << "Failed to create tab:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

QSqlDatabase openDb(const QString &password, const QString &dbName)
{
    const QString appName = QCoreApplication::applicationName();
    QSqlDatabase db = QSqlDatabase::addDatabase(dbType, appName + dbName);
    const QString path = QStringLiteral("%1/%2.db").arg(
        settingsDirectoryPath(), appName);
    db.setDatabaseName(path);

    qCDebug(logCategory) << "Opening database" << path;

    if (!db.open(dbUser, password)) {
        qCCritical(logCategory) << "Failed to open database" << db.lastError().text();
    } else if (!initializeSchema(db)) {
        qCCritical(logCategory) << "Failed to initialize database schema";
        db.close();
    }

    return db;
}

bool setDbPassword(QSqlDatabase &db, const QString &password)
{
    QSqlQuery query(db);
    const QString key = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString changePassword =
        QStringLiteral("PRAGMA rekey = \"x\'%1\'\";").arg(key);
    if (!query.exec(changePassword)) {
        qCCritical(logCategory) << "Failed to set password, error:" << query.lastError().text();
        return false;
    }
    qCDebug(logCategory) << "Database password changed";
    return true;
}

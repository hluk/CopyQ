// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

namespace KeychainAccess {

bool isEnabled();

QByteArray readPassword(const QString &service, const QString &key);

bool writePassword(const QString &service, const QString &key, const QByteArray &password);

bool deletePassword(const QString &service, const QString &key);

} // namespace KeychainAccess

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

namespace KeychainAccess {

QByteArray readPassword(const QString &service, const QString &key);

bool writePassword(const QString &service, const QString &key, const QByteArray &password);

void deletePassword(const QString &service, const QString &key);

} // namespace KeychainAccess

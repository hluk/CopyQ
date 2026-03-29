// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMap>
#include <QString>

/// Key-value map of paths used by the application.
QMap<QString, QString> pathMap();

/// Key-value map of diagnostic info: versions, features, platform, env vars.
QMap<QString, QString> infoMap();

/// Flat key-value text from infoMap(), one "key: value" per line, skipping empty values.
QString diagnosticText();

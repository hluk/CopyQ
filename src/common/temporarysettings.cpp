// SPDX-License-Identifier: GPL-3.0-or-later

#include "temporarysettings.h"

#include "common/log.h"
#include "common/temporaryfile.h"

#include <QTemporaryFile>

namespace {

QString temporaryFileName(const QByteArray &content)
{
    QTemporaryFile tmpfile;
    if ( !openTemporaryFile(&tmpfile, ".ini") )
    {
        log("Failed to create temporary settings file", LogError);
        return QString();
    }

    tmpfile.write(content);
    tmpfile.setAutoRemove(false);
    return tmpfile.fileName();
}

} // namespace

TemporarySettings::TemporarySettings(const QByteArray &content)
    : m_settings(new QSettings(temporaryFileName(content), QSettings::IniFormat))
{
}

TemporarySettings::~TemporarySettings()
{
    const QString fileName = m_settings->fileName();
    delete m_settings;
    m_settings = nullptr;

    if (!fileName.isEmpty())
        QFile::remove(fileName);
}

QSettings *TemporarySettings::settings()
{
    return m_settings;
}

QByteArray TemporarySettings::content()
{
    m_settings->sync();

    QFile file(m_settings->fileName());
    if (!file.open(QIODevice::ReadOnly)) {
        log("Failed to open temporary settings file", LogError);
        return QByteArray();
    }

    return file.readAll();
}

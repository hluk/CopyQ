// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptabletemporaryfile.h"

#include <QTemporaryFile>

Q_DECLARE_METATYPE(QTemporaryFile*)

ScriptableTemporaryFile::ScriptableTemporaryFile(const QString &path)
    : ScriptableFile(path)
{
}

bool ScriptableTemporaryFile::autoRemove()
{
    return tmpFile()->autoRemove();
}

QString ScriptableTemporaryFile::fileTemplate()
{
    return tmpFile()->fileTemplate();
}

void ScriptableTemporaryFile::setAutoRemove(bool autoRemove)
{
    tmpFile()->setAutoRemove(autoRemove);
}

void ScriptableTemporaryFile::setFileTemplate(const QJSValue &name)
{
    tmpFile()->setFileTemplate(name.toString());
}

QFile *ScriptableTemporaryFile::self()
{
    return tmpFile();
}

QTemporaryFile *ScriptableTemporaryFile::tmpFile()
{
    if (m_self)
        return m_self;

    m_self = new QTemporaryFile(this);
    setFile(m_self);
    return m_self;
}

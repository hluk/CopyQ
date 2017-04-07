/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "scriptabletemporaryfile.h"

#include <QTemporaryFile>

Q_DECLARE_METATYPE(QTemporaryFile*)

ScriptableTemporaryFile::ScriptableTemporaryFile(const QString &path)
    : ScriptableFile(path)
{
}

bool ScriptableTemporaryFile::autoRemove() const
{
    return m_self->autoRemove();
}

QString ScriptableTemporaryFile::fileTemplate() const
{
    return m_self->fileTemplate();
}

void ScriptableTemporaryFile::setAutoRemove(bool autoRemove)
{
    m_self->setAutoRemove(autoRemove);
}

void ScriptableTemporaryFile::setFileTemplate(const QString &name)
{
    m_self->setFileTemplate(name);
}

QFile *ScriptableTemporaryFile::self()
{
    if (m_self)
        return m_self;

    m_self = new QTemporaryFile(this);
    setFile(m_self);
    return m_self;
}

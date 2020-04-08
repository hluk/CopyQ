/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

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

#ifndef TEMPORARYFILEPROTOTYPE_H
#define TEMPORARYFILEPROTOTYPE_H

#include "fileprototype.h"

class QTemporaryFile;

class TemporaryFilePrototype final : public FilePrototype
{
    Q_OBJECT
public:
    explicit TemporaryFilePrototype(QObject *parent = nullptr);

public slots:
    bool autoRemove() const;
    QString fileTemplate() const;
    void setAutoRemove(bool autoRemove);
    void setFileTemplate(const QScriptValue &name);

private:
    QTemporaryFile *thisTemporaryFile() const;
};

#endif // TEMPORARYFILEPROTOTYPE_H

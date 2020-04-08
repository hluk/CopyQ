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

#include "filedialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QWidget>

FileDialog::FileDialog(QWidget *parent, const QString &caption, const QString &fileName)
    : QObject(parent)
    , m_parent(parent)
    , m_caption(caption)
    , m_defaultPath(QFileInfo(fileName).absoluteFilePath())
{
}

void FileDialog::exec()
{
    const QString fileName =
            QFileDialog::getSaveFileName(
                m_parent, m_caption, m_defaultPath,
                /* filter = */ QString(), /* selectedFilter = */ nullptr,
                QFileDialog::DontConfirmOverwrite | QFileDialog::ReadOnly);
    if (!fileName.isEmpty())
        emit fileSelected(fileName);
}

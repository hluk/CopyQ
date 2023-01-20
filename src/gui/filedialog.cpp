// SPDX-License-Identifier: GPL-3.0-or-later

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

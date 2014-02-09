/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDDIALOG_H
#define CLIPBOARDDIALOG_H

#include "item/clipboarditem.h"

#include <QDialog>
#include <QVariantMap>

class QListWidgetItem;

namespace Ui {
    class ClipboardDialog;
}

class ClipboardDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * Create dialog with item data or clipboard data content displayed.
     */
    explicit ClipboardDialog(const ClipboardItemPtr &item, QWidget *parent = NULL);
    ~ClipboardDialog();

signals:
    void changeClipboard(const QVariantMap &data);

private slots:
    void on_listWidgetFormats_currentItemChanged(
            QListWidgetItem *current, QListWidgetItem *previous);

    void on_actionRemove_Format_triggered();

    void onFinished();

private:
    Ui::ClipboardDialog *ui;
    ClipboardItemPtr m_item;
    QVariantMap m_data;
};

#endif // CLIPBOARDDIALOG_H

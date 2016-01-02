/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include <QDialog>
#include <QPersistentModelIndex>
#include <QPointer>
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
     * Create dialog with clipboard data.
     */
    explicit ClipboardDialog(QWidget *parent = NULL);

    /**
     * Create dialog with item data.
     */
    explicit ClipboardDialog(const QPersistentModelIndex &index, QAbstractItemModel *model, QWidget *parent = NULL);

    ~ClipboardDialog();

signals:
    void changeClipboard(const QVariantMap &data);

private slots:
    void on_listWidgetFormats_currentItemChanged(
            QListWidgetItem *current, QListWidgetItem *previous);

    void on_actionRemove_Format_triggered();

    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    void init();

    void setData(const QVariantMap &data);

    Ui::ClipboardDialog *ui;
    QPointer<QAbstractItemModel> m_model;
    QPersistentModelIndex m_index;
    QVariantMap m_data;
};

#endif // CLIPBOARDDIALOG_H

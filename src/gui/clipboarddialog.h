// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIPBOARDDIALOG_H
#define CLIPBOARDDIALOG_H

#include <QDialog>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QVariantMap>
#include <QTimer>

#include "platform/platformnativeinterface.h"

class QBuffer;
class QListWidgetItem;
class QMovie;

namespace Ui {
    class ClipboardDialog;
}

class ClipboardDialog final : public QDialog
{
    Q_OBJECT

public:
    /**
     * Create dialog with clipboard data.
     */
    explicit ClipboardDialog(QWidget *parent = nullptr);

    /**
     * Create dialog with item data.
     */
    explicit ClipboardDialog(const QPersistentModelIndex &index, QAbstractItemModel *model, QWidget *parent = nullptr);

    ~ClipboardDialog();

signals:
    void changeClipboard(const QVariantMap &data);

private:
    void onListWidgetFormatsCurrentItemChanged(
            QListWidgetItem *current, QListWidgetItem *previous);

    void onActionRemoveFormatTriggered();

    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    void addText();

    void init();

    void setData(const QVariantMap &data);

    Ui::ClipboardDialog *ui = nullptr;
    QPointer<QAbstractItemModel> m_model;
    QPersistentModelIndex m_index;
    QVariantMap m_data;
    bool m_dataFromClipboard = false;
    QString m_textToShow;
    QTimer m_timerTextLoad;

    QBuffer *m_animationBuffer = nullptr;
    QMovie *m_animation = nullptr;

    PlatformClipboardPtr m_clipboard;
};

#endif // CLIPBOARDDIALOG_H

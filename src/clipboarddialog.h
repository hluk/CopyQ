#ifndef CLIPBOARDDIALOG_H
#define CLIPBOARDDIALOG_H

#include <QDialog>
#include <QListWidgetItem>

namespace Ui {
    class ClipboardDialog;
}

class ClipboardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClipboardDialog(QWidget *parent = 0);
    ~ClipboardDialog();

private slots:
    void on_listWidgetFormats_currentItemChanged(
            QListWidgetItem *current, QListWidgetItem *previous);

private:
    Ui::ClipboardDialog *ui;
    QMap<QString, QByteArray> m_data;
};

#endif // CLIPBOARDDIALOG_H

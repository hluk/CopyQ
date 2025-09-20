// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QDialog>

namespace Ui {
class ShortcutDialog;
}

class ShortcutDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutDialog(QWidget *parent = nullptr);
    ~ShortcutDialog();

    /** Return accepted shortcut or empty one. */
    QKeySequence shortcut() const;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void onResetButtonClicked();

    Ui::ShortcutDialog *ui;
};

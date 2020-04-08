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

#ifndef SHORTCUTDIALOG_H
#define SHORTCUTDIALOG_H

#include <QDialog>

namespace Ui {
class ShortcutDialog;
}

class ShortcutDialog final : public QDialog
{
public:
    explicit ShortcutDialog(QWidget *parent = nullptr);
    ~ShortcutDialog();

    /** Return accepted shortcut or empty one. */
    QKeySequence shortcut() const;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void onResetButtonClicked();

    void processKey(int key, int mods);

    int getModifiers(const QKeyEvent &event);

    Ui::ShortcutDialog *ui;
    QKeySequence m_shortcut;
    bool m_metaPressed;
};

#endif // SHORTCUTDIALOG_H

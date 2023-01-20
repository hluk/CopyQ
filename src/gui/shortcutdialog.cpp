// SPDX-License-Identifier: GPL-3.0-or-later

#include "shortcutdialog.h"
#include "ui_shortcutdialog.h"

#include "common/log.h"
#include "common/shortcuts.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QPushButton>

ShortcutDialog::ShortcutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShortcutDialog)
{
    ui->setupUi(this);

    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);
    Q_ASSERT(resetButton);
    resetButton->setText(tr("Remove Shortcut"));
    connect(resetButton, &QAbstractButton::clicked,
            this, &ShortcutDialog::onResetButtonClicked);

    connect(ui->lineEditShortcut, &QKeySequenceEdit::keySequenceChanged,
            this, [this](const QKeySequence &keySequence) {
                if ( !keySequence.isEmpty() )
                    accept();
            });

    ui->lineEditShortcut->installEventFilter(this);

    setAttribute(Qt::WA_InputMethodEnabled, false);
}

ShortcutDialog::~ShortcutDialog()
{
    delete ui;
}

QKeySequence ShortcutDialog::shortcut() const
{
    return ui->lineEditShortcut->keySequence();
}

bool ShortcutDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object != ui->lineEditShortcut)
        return QDialog::eventFilter(object, event);

    // WORKAROUND: Meta keyboard modifier in QKeySequenceEdit is not handled
    // correctly on X11 and Wayland (QTBUG-78212, QTBUG-62102).
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();
        return key == Qt::Key_Meta
            || key == Qt::Key_Super_L
            || key == Qt::Key_Super_R
            || key == Qt::Key_Hyper_L
            || key == Qt::Key_Hyper_R;
    }

    return QDialog::eventFilter(object, event);
}

void ShortcutDialog::onResetButtonClicked()
{
    ui->lineEditShortcut->setKeySequence( QKeySequence() );
    accept();
}

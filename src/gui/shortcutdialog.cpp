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

#include "shortcutdialog.h"
#include "ui_shortcutdialog.h"

#include "common/log.h"
#include "common/shortcuts.h"
#include "gui/icons.h"
#include "platform/platformnativeinterface.h"

#include <QKeyEvent>
#include <QPushButton>

namespace {

bool isModifierKey(int key)
{
    switch(key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_unknown:
        return true;
    default:
        return false;
    }
}

} // namespace

ShortcutDialog::ShortcutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShortcutDialog)
    , m_shortcut()
    , m_metaPressed(false)
{
    ui->setupUi(this);

    QPushButton *resetButton = ui->buttonBox->button(QDialogButtonBox::Reset);
    Q_ASSERT(resetButton);
    resetButton->setText(tr("Remove Shortcut"));
    connect(resetButton, &QAbstractButton::clicked,
            this, &ShortcutDialog::onResetButtonClicked);

    ui->lineEditShortcut->installEventFilter(this);

    setAttribute(Qt::WA_InputMethodEnabled, false);
}

ShortcutDialog::~ShortcutDialog()
{
    delete ui;
}

QKeySequence ShortcutDialog::shortcut() const
{
    return m_shortcut;
}

bool ShortcutDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object != ui->lineEditShortcut)
        return QDialog::eventFilter(object, event);

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        const int key = platformNativeInterface()->keyCode(*keyEvent);
        const int mods = getModifiers(*keyEvent);

        if (mods == Qt::NoModifier) {
            if (key == Qt::Key_Tab)
                return QDialog::eventFilter(object, event);

            if (key == Qt::Key_Escape) {
                reject();
                return true;
            }
        }

        event->accept();

        if (event->type() == QEvent::KeyPress) {
            COPYQ_LOG(QString("Shortcut key press: %1").arg(keyEvent->key()));

            if ( isModifierKey(keyEvent->key()) ) {
                processKey(0, mods);
            } else {
                processKey(key, mods);
                accept();
            }
        } else if (result() != QDialog::Accepted) {
            COPYQ_LOG(QString("Shortcut key release: %1").arg(keyEvent->key()));
            processKey(0, mods);
        }

        return true;
    }

    return QDialog::eventFilter(object, event);
}

void ShortcutDialog::onResetButtonClicked()
{
    m_shortcut = QKeySequence();
    accept();
}

void ShortcutDialog::processKey(int key, int mods)
{
    m_shortcut = QKeySequence(mods | key);

    // WORKAROUND: Qt has convert some keys to upper case which
    //             breaks some shortcuts on some keyboard layouts.
    m_shortcut = QKeySequence(portableShortcutText(m_shortcut));

    const QString shortcut = m_shortcut.toString();
    COPYQ_LOG(QString("Shortcut: %1").arg(shortcut));

    ui->lineEditShortcut->setText(shortcut);
}

int ShortcutDialog::getModifiers(const QKeyEvent &event)
{
    int key = event.key();
    const Qt::KeyboardModifiers mods = event.modifiers();
    int result = 0;

    if (key == Qt::Key_Meta || key == Qt::Key_Super_L || key == Qt::Key_Super_R
            || key == Qt::Key_Hyper_L || key == Qt::Key_Hyper_R)
    {
        m_metaPressed = (event.type() == QEvent::KeyPress);
        COPYQ_LOG(QString("Shortcut \"Meta\" key %1.").arg(m_metaPressed ? "pressed" : "released"));
    }

    if (mods & Qt::ShiftModifier)
        result |= Qt::SHIFT;
    if (mods & Qt::ControlModifier)
        result |= Qt::CTRL;
    if (mods & Qt::AltModifier)
        result |= Qt::ALT;
    if (mods & Qt::KeypadModifier)
        result |= Qt::KeypadModifier;
    if (m_metaPressed || mods & Qt::MetaModifier)
        result |= Qt::META;

    return result;
}

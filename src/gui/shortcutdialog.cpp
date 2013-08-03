/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "common/client_server.h"
#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

#include <QKeyEvent>

namespace {

bool isKeyModifier(int key)
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
    setWindowIcon( getIcon("", IconHandUp) );
}

ShortcutDialog::~ShortcutDialog()
{
    delete ui;
}

QKeySequence ShortcutDialog::shortcut() const
{
    return m_shortcut;
}

void ShortcutDialog::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers mods = getModifiers(*event);

    if (mods == Qt::NoModifier) {
        if (key == Qt::Key_Escape)
            reject();
        else if (key == Qt::Key_Backspace)
            accept();
        return;
    }

    event->accept();
    processKey(key, mods);

    if ( !isKeyModifier(key) )
        accept();
}

void ShortcutDialog::keyReleaseEvent(QKeyEvent *event)
{
    Qt::KeyboardModifiers mods = getModifiers(*event);

    processKey(0, mods);
}

void ShortcutDialog::processKey(int key, Qt::KeyboardModifiers mods)
{
    if(key == Qt::Key_unknown)
        return;

    int keys = 0;
    if ( !isKeyModifier(key) )
        keys = key;

    if (mods & Qt::ControlModifier)
        keys += Qt::CTRL;
    if (mods & Qt::ShiftModifier)
        keys += Qt::SHIFT;
    if (mods & Qt::AltModifier)
        keys += Qt::ALT;
    if (mods & Qt::MetaModifier)
        keys += Qt::META;

    m_shortcut = QKeySequence(keys);
    QString shortcut = m_shortcut.toString(QKeySequence::NativeText);

    ui->label->setText(shortcut);
}

Qt::KeyboardModifiers ShortcutDialog::getModifiers(const QKeyEvent &event)
{
    int key = event.key();
    Qt::KeyboardModifiers mods = event.modifiers();

    if (key == Qt::Key_Super_L || key == Qt::Key_Super_R
            || key == Qt::Key_Hyper_L || key == Qt::Key_Hyper_R)
    {
        m_metaPressed = (event.type() == QEvent::KeyPress);
    }
    if (m_metaPressed)
        mods |= Qt::MetaModifier;

    return mods;

}

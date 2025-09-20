// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/navigation.h"

bool isViEscape(KeyMods keyMods)
{
    return keyMods.mods == Qt::ControlModifier && keyMods.key == Qt::Key_BracketLeft;
}

bool isEmacsEscape(KeyMods keyMods)
{
    return keyMods.mods == Qt::ControlModifier && keyMods.key == Qt::Key_G;
}

KeyMods translateToVi(KeyMods keyMods)
{
    const int key = keyMods.key;
    const Qt::KeyboardModifiers mods = keyMods.mods;

    switch ( key ) {
    case Qt::Key_G:
        return {
            mods & Qt::ShiftModifier ? Qt::Key_End : Qt::Key_Home,
            mods & ~Qt::ShiftModifier
        };
    case Qt::Key_J:
        return {Qt::Key_Down, mods};
    case Qt::Key_K:
        return {Qt::Key_Up, mods};
    case Qt::Key_L:
        return {Qt::Key_Return};
    case Qt::Key_F:
    case Qt::Key_D:
    case Qt::Key_B:
    case Qt::Key_U:
        if (mods & Qt::ControlModifier) {
            return {
                (key == Qt::Key_F || key == Qt::Key_D) ? Qt::Key_PageDown : Qt::Key_PageUp,
                mods & ~Qt::ControlModifier
            };
        }
        return {};
    case Qt::Key_X:
        return {Qt::Key_Delete};
    case Qt::Key_BracketLeft:
        if (mods & Qt::ControlModifier) {
            return {Qt::Key_Escape, mods & ~Qt::ControlModifier};
        }
        return {};
    default:
        return {};
    }
}

KeyMods translateToEmacs(KeyMods keyMods)
{
    const int key = keyMods.key;
    const Qt::KeyboardModifiers mods = keyMods.mods;

    switch ( key ) {
    case Qt::Key_V:
        if (mods & Qt::ControlModifier) {
            return {Qt::Key_PageDown, mods & ~Qt::ControlModifier};
        }
        if (mods & Qt::AltModifier) {
            return {Qt::Key_PageUp, mods & ~Qt::AltModifier};
        }
        return {};
    case Qt::Key_N:
        if (mods & Qt::ControlModifier) {
            return {Qt::Key_Down, mods & ~Qt::ControlModifier};
        }
        return {};
    case Qt::Key_P:
        if (mods & Qt::ControlModifier) {
            return {Qt::Key_Up, mods & ~Qt::ControlModifier};
        }
        return {};
    case Qt::Key_Less:
        if (mods & Qt::AltModifier) {
            return {Qt::Key_Home, mods & ~(Qt::ShiftModifier | Qt::AltModifier)};
        }
        return {};
    case Qt::Key_Greater:
        if ((mods & Qt::AltModifier)) {
            return {Qt::Key_End, mods & ~(Qt::ShiftModifier | Qt::AltModifier)};
        }
        return {};
    case Qt::Key_G:
        if (mods & Qt::ControlModifier) {
            return {Qt::Key_Escape, mods & ~Qt::ControlModifier};
        }
        return {};
    default:
        return {};
    }
}

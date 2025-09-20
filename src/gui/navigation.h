// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>

class QKeyEvent;
class QObject;

struct KeyMods {
    int key;
    Qt::KeyboardModifiers mods;

    KeyMods(int key = 0, Qt::KeyboardModifiers mods = Qt::NoModifier)
        : key(key), mods(mods) {}
};

bool isViEscape(KeyMods keyMods);
bool isEmacsEscape(KeyMods keyMods);

KeyMods translateToVi(KeyMods keyMods);
KeyMods translateToEmacs(KeyMods keyMods);

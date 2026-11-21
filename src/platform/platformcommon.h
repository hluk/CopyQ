// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

class AppConfig;
class PlatformWindow;

bool pasteWithCtrlV(PlatformWindow &window, const AppConfig &config);

void waitMs(int msec);

/**
 * Resolve the window to paste to when a clipboard item is activated.
 *
 * Normally this is the current foreground window (@a currentWindow), but if
 * that is one of the application's own windows (@a currentWindowIsOwn) or
 * unknown, fall back to @a lastForeignWindow - the last active window of
 * another application - so that pasting still targets the window the user
 * came from. If no foreign window is known, @a currentWindow is returned
 * unchanged (possibly null).
 */
std::shared_ptr<PlatformWindow> resolvePasteTargetWindow(
    const std::shared_ptr<PlatformWindow> &currentWindow,
    const std::shared_ptr<PlatformWindow> &lastForeignWindow,
    bool currentWindowIsOwn);

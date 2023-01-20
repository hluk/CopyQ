// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class AppConfig;
class PlatformWindow;

bool pasteWithCtrlV(PlatformWindow &window, const AppConfig &config);

void waitMs(int msec);

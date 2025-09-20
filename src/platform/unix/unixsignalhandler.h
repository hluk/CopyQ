// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


/**
 * Gracefully exit application on Unix signals SIGHUP, SIGINT and SIGTERM.
 *
 * More info at http://qt-project.org/doc/qt-4.8/unix-signals.html
 */

bool initUnixSignalHandler();
void startUnixSignalHandler();

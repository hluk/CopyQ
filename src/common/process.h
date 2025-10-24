// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class QProcess;

/**
 * Terminate process or kill if it takes too long.
 */
void terminateProcess(QProcess *p);

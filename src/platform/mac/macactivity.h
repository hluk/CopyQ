// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


class QString;

/**
 * RAII class for using activity blocks on OSX.
 */
class MacActivity
{
public:
    explicit MacActivity(const QString &reason);
    ~MacActivity();
private:
    void *m_activity;
};

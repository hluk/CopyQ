// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MACACTIVITY_H
#define MACACTIVITY_H

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

#endif // MACACTIVITY_H

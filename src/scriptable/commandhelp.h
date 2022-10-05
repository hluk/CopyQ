// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDHELP_H
#define COMMANDHELP_H

#include <QList>
#include <QString>

struct CommandHelp {
    CommandHelp();

    CommandHelp(const char *command, const QString &description);

    CommandHelp &addArg(const QString &arg);

    QString toString() const;

    QString cmd;
    QString desc;
    QString args;
};

QList<CommandHelp> commandHelp();

#endif // COMMANDHELP_H

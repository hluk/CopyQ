// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMANDSTATUS_H
#define COMMANDSTATUS_H

/** Command status. */
enum CommandStatus {
    /** Script finished */
    CommandFinished = 0,
    /** Script finished with exit code 1 (fail() was called) */
    CommandError = 1,
    /** Bad command syntax */
    CommandBadSyntax = 2,
    /** Exception thrown */
    CommandException = 4,

    /** Print on stdout */
    CommandPrint = 5,

    CommandFunctionCall = 8,
    CommandFunctionCallReturnValue = 9,

    CommandStop = 10,

    CommandInputDialogFinished = 11,

    CommandData = 12,

    CommandReceiveData = 13,
};

#endif // COMMANDSTATUS_H

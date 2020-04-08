/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

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

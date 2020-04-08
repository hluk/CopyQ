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
#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include "common/log.h"

#include <QDialog>

namespace Ui {
class LogDialog;
}

class Decorator;

class LogDialog final : public QDialog
{
public:
    explicit LogDialog(QWidget *parent = nullptr);
    ~LogDialog();

private:
    using FilterCheckBoxSlot = void (LogDialog::*)(bool);

    void updateLog();

    void showError(bool show);
    void showWarning(bool show);
    void showNote(bool show);
    void showDebug(bool show);
    void showTrace(bool show);

    void addFilterCheckBox(LogLevel level, FilterCheckBoxSlot slot);

    Ui::LogDialog *ui;

    Decorator *m_logDecorator;
    Decorator *m_stringDecorator;
    Decorator *m_threadNameDecorator;

    bool m_showError;
    bool m_showWarning;
    bool m_showNote;
    bool m_showDebug;
    bool m_showTrace;
};

#endif // LOGDIALOG_H

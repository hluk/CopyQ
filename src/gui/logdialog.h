// SPDX-License-Identifier: GPL-3.0-or-later
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

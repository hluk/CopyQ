// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TABDIALOG_H
#define TABDIALOG_H

#include <QDialog>
#include <QStringList>

namespace Ui {
    class TabDialog;
}

/**
 * Dialog for naming and renaming tabs.
 */
class TabDialog final : public QDialog
{
    Q_OBJECT

public:
    /** Tab dialog type (new tab or rename existing tab). */
    enum TabDialogType {
        TabNew,
        TabRename,
        TabGroupRename
    };

    explicit TabDialog(TabDialogType type, QWidget *parent = nullptr);
    ~TabDialog();

    /** Set tab index to rename (emitted parameter of accepted()). */
    void setTabIndex(int tabIndex);

    /** Set existing tabs for validation. */
    void setTabs(const QStringList &tabs);

    /** Set current tab name. */
    void setTabName(const QString &tabName);

    /** Set current tab group name. */
    void setTabGroupName(const QString &tabGroupName);

signals:
    void newTabNameAccepted(const QString &newName);
    void barTabNameAccepted(const QString &newName, int tabIndex);
    void treeTabNameAccepted(const QString &newName, const QString &oldName);

private:
    void onAccepted();

    /**
     * Validate tab name.
     * Tab name should be non-empty and should not be in existing tab list
     * (see setTabs()).
     */
    void validate();

    Ui::TabDialog *ui;
    int m_tabIndex = -1;
    QString m_tabGroupName;
    QStringList m_tabs;
};

#endif // TABDIALOG_H

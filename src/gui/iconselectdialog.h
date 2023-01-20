// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ICONSELECTDIALOG_H
#define ICONSELECTDIALOG_H

#include <QDialog>

class QModelIndex;

class IconListWidget;

class IconSelectDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit IconSelectDialog(const QString &defaultIcon, QWidget *parent = nullptr);

    const QString &selectedIcon() const { return m_selectedIcon; }

    void done(int result) override;

signals:
    void iconSelected(const QString &icon);

private:
    void onIconListItemActivated(const QModelIndex &index);

    void onBrowse();

    void onAcceptCurrent();

    void addIcons();

    IconListWidget *m_iconList;
    QString m_selectedIcon;
};

#endif // ICONSELECTDIALOG_H

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QWidget>

class QString;

class IconWidget final : public QWidget
{
public:
    explicit IconWidget(int icon, QWidget *parent = nullptr);

    explicit IconWidget(const QString &icon, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_icon;
};

// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ICONWIDGET_H
#define ICONWIDGET_H

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

#endif // ICONWIDGET_H

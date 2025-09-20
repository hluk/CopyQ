// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QPushButton>

class IconSelectButton final : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QString currentIcon READ currentIcon WRITE setCurrentIcon NOTIFY currentIconChanged)
public:
    explicit IconSelectButton(QWidget *parent = nullptr);

    const QString &currentIcon() const { return m_currentIcon; }

    QSize sizeHint() const override;

    void setCurrentIcon(const QString &iconString);

signals:
    void currentIconChanged(const QString &icon);

private:
    void onClicked();

    QString m_currentIcon;
};

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <QObject>
#include <QString>

class QWidget;

/**
 * Opens native file dialog (unlike QFileDialog).
 */
class FileDialog final : public QObject
{
    Q_OBJECT
public:
    FileDialog(QWidget *parent, const QString &caption, const QString &fileName);

    void exec();

signals:
    void fileSelected(const QString &fileName);

private:
    QWidget *m_parent;
    QString m_caption;
    QString m_defaultPath;
};

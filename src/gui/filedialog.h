// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

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

#endif // FILEDIALOG_H

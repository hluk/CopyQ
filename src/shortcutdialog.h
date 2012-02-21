#ifndef SHORTCUTDIALOG_H
#define SHORTCUTDIALOG_H

#include <QDialog>

namespace Ui {
class ShortcutDialog;
}

class ShortcutDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ShortcutDialog(QWidget *parent = 0);
    ~ShortcutDialog();

    QKeySequence shortcut() const;

protected:
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    
private:
    Ui::ShortcutDialog *ui;
    QKeySequence m_shortcut;

    void processKey(int key, Qt::KeyboardModifiers mods);
};

#endif // SHORTCUTDIALOG_H

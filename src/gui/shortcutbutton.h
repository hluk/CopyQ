// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SHORTCUTBUTTON_H
#define SHORTCUTBUTTON_H

#include <QKeySequence>
#include <QList>
#include <QToolBar>

class QAction;
class QHBoxLayout;

/**
 * Widget with buttons for defining shortcuts and single button for adding shortcuts.
 */
class ShortcutButton final : public QToolBar
{
    Q_OBJECT
public:
    explicit ShortcutButton(QWidget *parent = nullptr);

    /** Creates new shortcut button for @a shortcut if it's valid and same button doesn't exist. */
    void addShortcut(const QKeySequence &shortcut);

    /**
     * Overloaded method.
     *
     * Creates new shortcut from string formatted as QKeySequence::PortableText (if valid).
     */
    void addShortcut(const QString &shortcutPortableText);

    /** Remove all shortcut buttons. */
    void clearShortcuts();

    /** Remove all shortcut buttons and add button with default shortcut if valid. */
    void resetShortcuts();

    /** Set default shortcut (after reset()). */
    void setDefaultShortcut(const QKeySequence &defaultShortcut);

    /** Return valid shortcuts defined by buttons. */
    QList<QKeySequence> shortcuts() const;

    /** Add icon and tooltip to buttons that contain shortcut from @a ambiguousShortcuts list. */
    void checkAmbiguousShortcuts(const QList<QKeySequence> &ambiguousShortcuts,
                                 const QIcon &warningIcon, const QString &warningToolTip);

signals:
    /** Emitted if new @a shortcut (with button) was added. */
    void shortcutAdded(const QKeySequence &shortcut);
    /** Emitted if @a shortcut (with button) was removed. */
    void shortcutRemoved(const QKeySequence &shortcut);

protected:
    void showEvent(QShowEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void onShortcutButtonClicked();
    void onButtonAddShortcutClicked();
    void addShortcut(QAction *shortcutButton);
    void setButtonShortcut(QAction *shortcutButton, const QKeySequence &shortcut);

    QKeySequence shortcutForButton(const QAction &w) const;

    bool focusNextPrevious(bool next);

    QKeySequence m_defaultShortcut;
    QAction *m_actionAddShortcut;
};

#endif // SHORTCUTBUTTON_H

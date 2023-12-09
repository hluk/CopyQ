// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TRAYMENU_H
#define TRAYMENU_H

#include <QMenu>
#include <QPointer>
#include <QTimer>

class QAction;
class QModelIndex;

class TrayMenu final : public QMenu
{
    Q_OBJECT
public:
    explicit TrayMenu(QWidget *parent = nullptr);

    static void updateTextFromData(QAction *act, const QVariantMap &data);
    static bool updateIconFromData(QAction *act, const QVariantMap &data);

    /**
     * Add clipboard item action with number key hint.
     *
     * Triggering this action emits clipboardItemActionTriggered() signal.
     */
    QAction *addClipboardItemAction(const QVariantMap &data, bool showImages);

    void clearClipboardItems();

    void clearCustomActions();

    /** Add custom action. */
    void setCustomActions(QList<QAction*> actions);

    /** Clear clipboard item actions and curstom actions. */
    void clearAllActions();

    /** Handle Vi shortcuts. */
    void setViModeEnabled(bool enabled);

    /** Enable searching for numbers. */
    void setNumberSearchEnabled(bool enabled);

    /** Filter clipboard items. */
    void search(const QString &text);

    void markItemInClipboard(const QVariantMap &clipboardData);

    /** Row numbers start from one instead of zero? */
    void setRowIndexFromOne(bool rowIndexFromOne) { m_rowIndexFromOne = rowIndexFromOne; }

signals:
    /** Emitted if numbered action triggered. */
    void clipboardItemActionTriggered(const QVariantMap &itemData, bool omitPaste);

    void searchRequest(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void actionEvent(QActionEvent *event) override;

    void leaveEvent(QEvent *event) override;

    void inputMethodEvent(QInputMethodEvent *event) override;

private:
    void onClipboardItemActionTriggered();

    void delayedUpdateActiveAction();
    void doUpdateActiveAction();

    void setSearchMenuItem(const QString &text);

    QPointer<QAction> m_clipboardItemActionsSeparator;
    QPointer<QAction> m_customActionsSeparator;
    QPointer<QAction> m_searchAction;
    int m_clipboardItemActionCount;

    bool m_omitPaste;
    bool m_viMode;
    bool m_numberSearch;

    QString m_searchText;

    QTimer m_timerUpdateActiveAction;

    bool m_rowIndexFromOne = true;

    QList<QAction*> m_clipboardActions;
    QList<QAction*> m_customActions;
};

#endif // TRAYMENU_H

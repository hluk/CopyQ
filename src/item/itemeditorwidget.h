// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMEDITORWIDGET_H
#define ITEMEDITORWIDGET_H

#include "item/itemfilter.h"
#include "item/itemwidget.h"
#include "gui/menuitems.h"

#include <QPersistentModelIndex>
#include <QTextEdit>

#include <memory>

class ItemWidget;
class QAbstractItemModel;
class QPlainTextEdit;
class QTextCursor;
class QTextDocument;
class QWidget;

/**
 * Internal editor widget for items.
 */
class ItemEditorWidget final : public QTextEdit
{
    Q_OBJECT
public:
    ItemEditorWidget(const QModelIndex &index, const QString &format, QWidget *parent = nullptr);

    bool isValid() const;

    bool hasChanges() const;

    void setHasChanges(bool hasChanges);

    void setSaveOnReturnKey(bool enabled);

    QVariantMap data() const;

    QModelIndex index() const { return m_index; }

    void search(const ItemFilterPtr &filter);

    void findNext();

    void findPrevious();

    QWidget *createToolbar(QWidget *parent, const MenuItems &menuItems);

signals:
    void save();
    void cancel();
    void invalidate();
    void searchRequest();

protected:
    void keyPressEvent(QKeyEvent *event) override;

    bool canInsertFromMimeData(const QMimeData *source) const override;

    void insertFromMimeData(const QMimeData *source) override;

private:
    void saveAndExit();

    void changeSelectionFont();
    void toggleBoldText();
    void toggleItalicText();
    void toggleUnderlineText();
    void toggleStrikethroughText();
    void setForeground();
    void setBackground();
    void eraseStyle();

    void search(bool backwards);

    QPersistentModelIndex m_index;
    bool m_saveOnReturnKey;
    QString m_format;
    ItemFilterPtr m_filter;
};

#endif // ITEMEDITORWIDGET_H

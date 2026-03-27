// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include "gui/icons.h"
#include "item/itemwidgetwrapper.h"

#include <QVariant>
#include <QWidget>
#include <QColor>

namespace Ui {
class ItemNotesSettings;
}

class QHBoxLayout;
class QTextEdit;
class QTimer;

enum NotesPosition {
    NotesAbove,
    NotesBelow,
    NotesBeside,
};

class ItemNotes final : public QWidget, public ItemWidgetWrapper
{
    Q_OBJECT
    Q_PROPERTY(QColor notesIndicatorColor READ notesIndicatorColor
               WRITE setNotesIndicatorColor)
    Q_PROPERTY(int notesIndicatorWidth READ notesIndicatorWidth
               WRITE setNotesIndicatorWidth)

public:
    ItemNotes(ItemWidget *childItem, const QString &text, const QByteArray &icon,
              NotesPosition notesPosition, bool showToolTip);

    void setCurrent(bool current) override;

    QColor notesIndicatorColor() const { return m_indicatorColor; }
    void setNotesIndicatorColor(const QColor &color) { m_indicatorColor = color; }

    int notesIndicatorWidth() const { return m_indicatorWidth; }
    void setNotesIndicatorWidth(int width);

protected:
    void updateSize(QSize maximumSize, int idealWidth) override;

    void paintEvent(QPaintEvent *event) override;

    bool eventFilter(QObject *, QEvent *event) override;

private:
    void showToolTip();

    QTextEdit *m_notes;
    QWidget *m_icon;
    QHBoxLayout *m_labelLayout;
    QTimer *m_timerShowToolTip;
    QString m_toolTipText;
    bool m_isCurrent = false;
    QColor m_indicatorColor;
    int m_indicatorWidth = 0;
};

class ItemNotesLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemNotesLoader();
    ~ItemNotesLoader();

    QString id() const override { return "itemnotes"; }
    QString name() const override { return tr("Notes"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Display notes for items."); }
    QVariant icon() const override { return QVariant(IconPenToSquare); }

    QStringList formatsToSave() const override;

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

    ItemWidget *transform(ItemWidget *itemWidget, const QVariantMap &data) override;

    bool matches(const QModelIndex &index, const ItemFilter &filter) const override;

private:
    bool m_notesAtBottom = false;
    bool m_notesBeside = false;
    bool m_showTooltip = false;
    std::unique_ptr<Ui::ItemNotesSettings> ui;
};

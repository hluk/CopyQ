// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ITEMTEXT_H
#define ITEMTEXT_H

#include "gui/icons.h"
#include "item/itemwidget.h"

#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>

#include <memory>

namespace Ui {
class ItemTextSettings;
}

class ItemText final : public QTextEdit, public ItemWidget
{
    Q_OBJECT

public:
    ItemText(
        const QString &text,
        const QString &richText,
        const QString &defaultStyleSheet,
        int maxLines,
        int lineLength,
        int maximumHeight,
        QWidget *parent);

protected:
    void updateSize(QSize maximumSize, int idealWidth) override;

    bool eventFilter(QObject *, QEvent *event) override;

    QMimeData *createMimeDataFromSelection() const override;

private:
    void onSelectionChanged();

    QTextDocument m_textDocument;
    QTextDocumentFragment m_elidedFragment;
    int m_ellipsisPosition = -1;
    int m_maximumHeight;
    bool m_isRichText = false;
};

class ItemTextLoader final : public QObject, public ItemLoaderInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID COPYQ_PLUGIN_ITEM_LOADER_ID)
    Q_INTERFACES(ItemLoaderInterface)

public:
    ItemTextLoader();
    ~ItemTextLoader();

    ItemWidget *create(const QVariantMap &data, QWidget *parent, bool preview) const override;

    QString id() const override { return "itemtext"; }
    QString name() const override { return tr("Text"); }
    QString author() const override { return QString(); }
    QString description() const override { return tr("Display plain text and simple HTML items."); }
    QVariant icon() const override { return QVariant(IconFont); }

    QStringList formatsToSave() const override;

    void applySettings(QSettings &settings) override;

    void loadSettings(const QSettings &settings) override;

    QWidget *createSettingsWidget(QWidget *parent) override;

private:
    bool m_useRichText = true;
    int m_maxLines = 0;
    int m_maxHeight = 0;
    QString m_defaultStyleSheet;
    std::unique_ptr<Ui::ItemTextSettings> ui;
};

#endif // ITEMTEXT_H

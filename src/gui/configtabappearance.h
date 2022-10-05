// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIGTABAPPEARANCE_H
#define CONFIGTABAPPEARANCE_H

#include "gui/theme.h"

#include <QWidget>
#include <QTimer>

namespace Ui {
class ConfigTabAppearance;
}

class ItemDelegate;
class ItemFactory;
class Option;
class QAbstractScrollArea;
class QSettings;

class ConfigTabAppearance final : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigTabAppearance(QWidget *parent = nullptr);
    ~ConfigTabAppearance();

    /** Load theme from settings file. */
    void loadTheme(const QSettings &settings);
    /** Save theme to settings file. */
    void saveTheme(QSettings *settings);

    void setEditor(const QString &editor) { m_editor = editor; }

    void createPreview(ItemFactory *itemFactory);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onFontButtonClicked();
    void onColorButtonClicked();

    void onPushButtonLoadThemeClicked();
    void onPushButtonSaveThemeClicked();
    void onPushButtonResetThemeClicked();
    void onPushButtonEditThemeClicked();

    void onCheckBoxShowNumberStateChanged(int);
    void onCheckBoxScrollbarsStateChanged(int);
    void onCheckBoxAntialiasStateChanged(int);

    void onComboBoxThemesActivated(const QString &text);

    void onThemeModified(const QByteArray &bytes);

    void updateThemes();
    void addThemes(const QString &path);
    void updateStyle();

    void fontButtonClicked(QObject *button);
    void colorButtonClicked(QObject *button);

    void updateColorButtons();
    void updateFontButtons();

    QIcon createThemeIcon(const QString &fileName);

    void decoratePreview();
    void decoratePreviewNow();

    Ui::ConfigTabAppearance *ui;
    Theme m_theme;
    QString m_editor;

    QWidget *m_preview = nullptr;
    ItemFactory *m_itemFactory = nullptr;

    QTimer m_timerPreview;
};

#endif // CONFIGTABAPPEARANCE_H

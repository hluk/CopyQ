/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CONFIGTABAPPEARANCE_H
#define CONFIGTABAPPEARANCE_H

#include "gui/theme.h"

#include <QWidget>

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

    Ui::ConfigTabAppearance *ui;
    Theme m_theme;
    QString m_editor;

    QWidget *m_preview = nullptr;
    ItemFactory *m_itemFactory = nullptr;
};

#endif // CONFIGTABAPPEARANCE_H

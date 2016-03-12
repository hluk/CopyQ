/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#ifndef THEME_H
#define THEME_H

#include "common/option.h"

#include <QHash>

namespace Ui {
class ConfigTabAppearance;
}

class QAbstractScrollArea;
class ItemDelegate;
class QListView;
class QSettings;
class QString;
class QVariant;

class Theme {
public:
    Theme();
    explicit Theme(QSettings &settings);
    explicit Theme(Ui::ConfigTabAppearance *ui);

    /** Load theme from settings file. */
    void loadTheme(QSettings &settings);
    /** Save theme to settings file. */
    void saveTheme(QSettings &settings) const;

    /** Return value for theme option with given @a name. */
    QVariant value(const QString &name) const;

    /** Return parsed color. */
    QColor color(const QString &name) const;

    /** Return parsed font. */
    QFont font(const QString &name) const;

    /** Set fonts and color for ClipboardBrowser object. */
    void decorateBrowser(QListView *c, ItemDelegate *d) const;

    /** Decorate main window. */
    void decorateMainWindow(QWidget *mainWindow) const;

    /** Decorate tool bar. */
    void decorateToolBar(QWidget *toolBar) const;

    /** Decorate scroll area (toggle scroll bar). */
    void decorateScrollArea(QAbstractScrollArea *scrollArea) const;

    /** Return stylesheet for tooltips. */
    QString getToolTipStyleSheet() const;

    /** Return stylesheet for notifications. */
    QString getNotificationStyleSheet() const;

    QFont themeFontFromString(const QString &fontString) const;

    bool isAntialiasingEnabled() const;

    void resetTheme();

private:
    bool isMainWindowThemeEnabled() const;

    /** Return style sheet with given @a name. */
    QString themeStyleSheet(const QString &name) const;

    /** Return parsed color name. */
    QString themeColorString(const QString &name) const;

    QHash<QString, Option> m_theme;
    Ui::ConfigTabAppearance *ui;
};

QString serializeColor(const QColor &color);

QColor deserializeColor(const QString &colorName);

QColor evalColor(const QString &expression, const Theme &theme, int maxRecursion = 8);

#endif // THEME_H

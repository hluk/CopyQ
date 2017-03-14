/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include <QFont>
#include <QHash>
#include <QPalette>

namespace Ui {
class ConfigTabAppearance;
}

class QAbstractScrollArea;
class QListView;
class QSettings;
class QString;
class QVariant;

struct ClipboardBrowserShared;

class Theme {
public:
    Theme() = default;
    explicit Theme(const QSettings &settings);
    explicit Theme(Ui::ConfigTabAppearance *ui);

    /** Load theme from settings file. */
    void loadTheme(const QSettings &settings);
    /** Save theme to settings file. */
    void saveTheme(QSettings *settings) const;

    /** Return value for theme option with given @a name. */
    QVariant value(const QString &name) const;

    /** Return parsed color. */
    QColor color(const QString &name) const;

    /** Return parsed font. */
    QFont font(const QString &name) const;

    /** Returt evaluated color expression. */
    QColor evalColorExpression(const QString &expr) const;

    /** Set fonts and color for ClipboardBrowser object. */
    void decorateBrowser(QListView *c) const;

    /** Decorate main window. */
    void decorateMainWindow(QWidget *mainWindow) const;

    /** Decorate tool bar. */
    void decorateToolBar(QWidget *toolBar) const;

    /** Decorate scroll area (toggle scroll bar). */
    void decorateScrollArea(QAbstractScrollArea *scrollArea) const;

    /** Decorate item preview. */
    void decorateItemPreview(QAbstractScrollArea *itemPreview) const;

    /** Return stylesheet for tooltips. */
    QString getToolTipStyleSheet() const;

    /** Return stylesheet for notifications. */
    QString getNotificationStyleSheet() const;

    QFont themeFontFromString(const QString &fontString) const;

    bool isAntialiasingEnabled() const;

    void resetTheme();

    bool showRowNumber() const { return m_showRowNumber; }
    const QFont &rowNumberFont() const { return m_rowNumberFont; }
    const QPalette &rowNumberPalette() const { return m_rowNumberPalette; }
    QSize rowNumberSize() const { return m_rowNumberSize; }

    const QFont &editorFont() const { return m_editorFont; }
    const QPalette &editorPalette() const { return m_editorPalette; }

    const QFont &searchFont() const { return m_searchFont; }
    const QPalette &searchPalette() const { return m_searchPalette; }

    QSize margins() const { return m_margins; }

private:
    void decorateBrowser(QAbstractScrollArea *c) const;

    bool isMainWindowThemeEnabled() const;

    /** Return style sheet with given @a name. */
    QString themeStyleSheet(const QString &name) const;

    /** Return parsed color name. */
    QString themeColorString(const QString &name) const;

    QString getMenuStyleSheet(const QString &selector) const;

    QHash<QString, Option> m_theme;
    Ui::ConfigTabAppearance *ui = nullptr;

    QFont m_rowNumberFont;
    QSize m_rowNumberSize;
    QPalette m_rowNumberPalette;
    bool m_showRowNumber = false;

    QFont m_editorFont;
    QPalette m_editorPalette;

    QFont m_searchFont;
    QPalette m_searchPalette;

    bool m_antialiasing = true;
    QSize m_margins;
};

QString serializeColor(const QColor &color);

QColor deserializeColor(const QString &colorName);

QColor evalColor(const QString &expression, const Theme &theme, int maxRecursion = 8);

#endif // THEME_H

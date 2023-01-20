// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THEME_H
#define THEME_H

#include "common/option.h"

#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QPalette>
#include <QStringList>

namespace Ui {
class ConfigTabAppearance;
}

class QAbstractScrollArea;
class QListView;
class QSettings;
class QString;
class QVariant;

struct ClipboardBrowserShared;

using Values = QHash<QString, QVariant>;

class Theme final {
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

    /** Decorate scroll area (toggle scroll bar). */
    void decorateScrollArea(QAbstractScrollArea *scrollArea) const;

    /** Decorate item preview. */
    void decorateItemPreview(QAbstractScrollArea *itemPreview) const;

    /** Return stylesheet for menus. */
    QString getMenuStyleSheet() const;

    QString getNotificationStyleSheet() const;

    Qt::ScrollBarPolicy scrollbarPolicy() const;

    bool useSystemIcons() const;

    QFont themeFontFromString(const QString &fontString) const;

    bool isAntialiasingEnabled() const;

    void resetTheme();

    void updateTheme();

    QSize rowNumberSize(int n) const;
    bool showRowNumber() const { return m_showRowNumber; }
    const QFont &rowNumberFont() const { return m_rowNumberFont; }
    const QPalette &rowNumberPalette() const { return m_rowNumberPalette; }

    const QFont &editorFont() const { return m_editorFont; }
    const QPalette &editorPalette() const { return m_editorPalette; }

    const QFont &searchFont() const { return m_searchFont; }
    const QPalette &searchPalette() const { return m_searchPalette; }

    QSize margins() const { return m_margins; }

    void setRowIndexFromOne(bool enabled) { m_rowIndexFromOne = enabled; }

private:
    void decorateBrowser(QAbstractScrollArea *c) const;

    bool isMainWindowThemeEnabled() const;

    /** Return style sheet with given @a name. */
    QString themeStyleSheet(const QString &name) const;

    /** Return parsed color name. */
    QString themeColorString(const QString &name) const;

    QString getStyleSheet(const QString &name, Values values = Values(), int maxRecursion = 8) const;
    QString parseStyleSheet(const QString &css, Values values, int maxRecursion) const;
    QString parsePlaceholder(const QString &name, Values *values, int maxRecursion) const;

    QHash<QString, Option> m_theme;
    Ui::ConfigTabAppearance *ui = nullptr;

    QFont m_rowNumberFont;
    QFontMetrics m_rowNumberFontMetrics = QFontMetrics(m_rowNumberFont);
    QPalette m_rowNumberPalette;
    bool m_showRowNumber = false;
    int m_rowNumberMargin = 2;

    QFont m_editorFont;
    QPalette m_editorPalette;

    QFont m_searchFont;
    QPalette m_searchPalette;

    bool m_antialiasing = true;
    QSize m_margins;

    bool m_rowIndexFromOne = true;
};

QString serializeColor(const QColor &color);

QColor deserializeColor(const QString &colorName);

QColor evalColor(const QString &expression, const Theme &theme, const Values &values = Values(), int maxRecursion = 8);

QString findThemeFile(const QString &fileName);

QString defaultUserThemePath();

QStringList themePaths();

#endif // THEME_H

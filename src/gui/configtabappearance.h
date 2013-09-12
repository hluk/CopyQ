/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include <QHash>
#include <QWidget>

namespace Ui {
class ConfigTabAppearance;
}

class ClipboardBrowser;
class Option;
class QSettings;

class ConfigTabAppearance : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigTabAppearance(QWidget *parent = NULL);
    ~ConfigTabAppearance();

    /** Load theme from settings file. */
    void loadTheme(QSettings &settings);
    /** Save theme to settings file. */
    void saveTheme(QSettings &settings) const;

    /** Return value for theme option with given @a name. */
    QVariant themeValue(const QString &name) const;

    /** Set fonts and color for ClipboardBrowser object. */
    void decorateBrowser(ClipboardBrowser *c) const;

    /** Decorate tab widget. */
    void decorateTabs(QWidget *tabWidget) const;

    /** Return stylesheet for tooltips. */
    QString getToolTipStyleSheet() const;

    void setEditor(const QString &editor) { m_editor = editor; }

    /** Return parsed color. */
    QColor themeColor(const QString &name) const;

    /** Return parsed color. */
    QFont themeFont(const QString &name) const;

protected:
    void showEvent(QShowEvent *event);

private slots:
    void onFontButtonClicked();
    void onColorButtonClicked();

    void on_pushButtonLoadTheme_clicked();
    void on_pushButtonSaveTheme_clicked();
    void on_pushButtonResetTheme_clicked();
    void on_pushButtonEditTheme_clicked();

    void on_checkBoxShowNumber_stateChanged(int);
    void on_checkBoxScrollbars_stateChanged(int);
    void on_checkBoxAntialias_stateChanged(int);

    void on_comboBoxThemes_activated(const QString &text);

    void onThemeModified(const QByteArray &bytes);

private:
    void updateTheme(QSettings &settings, QHash<QString, Option> *theme);
    void updateThemes();

    void fontButtonClicked(QObject *button);
    void colorButtonClicked(QObject *button);

    void updateColorButtons();
    void updateFontButtons();

    QFont themeFontFromString(const QString &fontString) const;

    /** Return parsed color name. */
    QString themeColorString(const QString &name) const;
    /** Return style sheet with given @a name. */
    QString themeStyleSheet(const QString &name) const;

    void initThemeOptions();
    QString defaultUserThemePath() const;
    QVariant themeValue(const QString &name, const QHash<QString, Option> &theme) const;
    QColor themeColor(const QString &name, const QHash<QString, Option> &theme) const;
    QIcon createThemeIcon(const QString &fileName);

    Ui::ConfigTabAppearance *ui;
    QHash<QString, Option> m_theme;
    QString m_editor;
};

#endif // CONFIGTABAPPEARANCE_H

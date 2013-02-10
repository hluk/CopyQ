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

#ifndef ITEMWIDGET_H
#define ITEMWIDGET_H

#include <QRegExp>
#include <QSize>
#include <QtPlugin>

class QFont;
class QModelIndex;
class QPalette;
class QWidget;

/**
 * Handles item in list.
 */
class ItemWidget
{
public:
    explicit ItemWidget(QWidget *widget);

    virtual ~ItemWidget() {}

    /**
     * Set search and selections highlight color and font.
     */
    void setHighlight(const QRegExp &re, const QFont &highlightFont,
                      const QPalette &highlightPalette);

    const QSize &size() const { return m_size; }

    void setMaximumSize(const QSize& size);

    virtual void updateItem();

    /**
     * Return widget to render.
     */
    virtual QWidget *widget() = 0;

    /**
     * Set data to display.
     */
    virtual void setData(const QModelIndex &index) = 0;

    /**
     * Create editor widget with given @a parent.
     */
    virtual QWidget *createEditor(QWidget *parent) const;

    /**
     * Set data for editor widget.
     */
    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;

protected:
    /**
     * Highlight matching text with given font and color.
     * Default implementation does nothing.
     */
    virtual void highlight(const QRegExp &, const QFont &, const QPalette &) {}

    /**
     * Size of widget needs to be updated (because maximum size chaged).
     */
    virtual void updateSize() = 0;

private:
    QRegExp m_re;
    QSize m_size;
};

class ItemLoaderInterface
{
public:
    ItemLoaderInterface() : m_enabled(true) {}
    virtual ~ItemLoaderInterface() {}

    /**
     * Create ItemWidget instance from index data.
     *
     * @return NULL if index hasn't appropriate data
     */
    virtual ItemWidget *create(const QModelIndex &index, QWidget *parent) = 0;

    virtual QString name() const = 0;
    virtual QString author() const = 0;
    virtual QString description() const = 0;

    void setEnabled(bool enabled) { m_enabled = enabled; }

    bool isEnabled() const { return m_enabled; }
private:
    bool m_enabled;
};

Q_DECLARE_INTERFACE(ItemLoaderInterface, "org.CopyQ.ItemPlugin.ItemLoader/1.0")

#endif // ITEMWIDGET_H

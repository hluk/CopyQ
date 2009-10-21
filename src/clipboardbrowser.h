/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of Copyq.

    Copyq is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CLIPBOARDBROWSER_H
#define CLIPBOARDBROWSER_H

#include <QListWidget>
#include <QRegExp>
#include <QClipboard>
#include <QBasicTimer>
#include <QSettings>
#include "qeditor.h"

class ClipboardBrowser : public QListWidget
{
    Q_OBJECT

    public:
        ClipboardBrowser(QWidget *parent = 0);
        ~ClipboardBrowser();
        void readSettings();
        void writeSettings();
        bool add(const QString &txt);
        bool remove();
        bool isCurrent(const QString &txt);

    private:
        QStringList m_newlst;
        QClipboard *m_clip;
        QRegExp m_search;
        bool m_ctrlmod;
        bool m_shiftmod;
        int m_maxitems;
        QString m_editor;
        QBasicTimer timer;

    protected:
        void keyPressEvent(QKeyEvent *event);
        void keyReleaseEvent(QKeyEvent *event);
        void timerEvent(QTimerEvent *event);

    signals:
        void requestSearch(QEvent *event);
        void hideSearch();
        void escapePressed();
        void closeAllEditors();

    public slots:
        void keyEvent(QKeyEvent *event) { QListWidget::keyPressEvent(event); };
        void clipboardChanged(QClipboard::Mode);
        void moveToClipboard(const QString &str);
        void moveToClipboard(QListWidgetItem *item);
        void filterItems(const QString &str);
        void itemModified(uint hash, const QString &str);

    private slots:
        void on_itemChanged(QListWidgetItem *item);
        void closeEditor(QEditor *editor);
        void openEditor();
};

#endif // CLIPBOARDBROWSER_H

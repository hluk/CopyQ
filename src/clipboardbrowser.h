/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

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

#ifndef CLIPBOARDBROWSER_H
#define CLIPBOARDBROWSER_H

#include <QListView>
#include <QRegExp>
#include <QClipboard>
#include <QBasicTimer>
#include <QSettings>
#include "qeditor.h"

class ActionDialog;
class ClipboardModel;
class ItemDelegate;

class ClipboardBrowser : public QListView
{
    Q_OBJECT

    public:
        ClipboardBrowser(QWidget *parent = 0);
        ~ClipboardBrowser();
        void readSettings(const QString &css);
        void writeSettings();
        void saveItems();
        bool add(const QString &txt, bool ignore_empty = true);
        void remove();
        QString itemText(int i = -1) const;
        QString itemText(QModelIndex ind) const;
        void sync(bool list_to_clipboard = true);
        QModelIndex index(int i) const {
            return model()->index(i,0);
        }
        void setCurrent(int row, bool cycle = false);

        void startMonitoring();
        void stopMonitoring();

    private:
        QClipboard *m_clip;
        int m_maxitems;
        QString m_editor;
        QBasicTimer timer;
        QBasicTimer timer_save;
        ClipboardModel *m;
        ItemDelegate *d;

        // items data file
        QSettings::Format datFormat;
        QSettings datSettings;
        ActionDialog *actionDialog;

    protected:
        void keyPressEvent(QKeyEvent *event);
        void timerEvent(QTimerEvent *event);
        void dataChanged(const QModelIndex &first, const QModelIndex &last);

    signals:
        void requestSearch(QEvent *event);
        void hideSearch();
        void escapePressed();
        void closeAllEditors();
        void error(const QString);

    public slots:
        void keyEvent(QKeyEvent *event) { keyPressEvent(event); };
        void clipboardChanged(QClipboard::Mode);
        void moveToClipboard(const QModelIndex &ind);
        void moveToClipboard(const QString &str);
        void moveToClipboard(int i);
        void filterItems(const QString &str);
        void itemModified(uint hash, const QString &str);
        void addItems(const QStringList &items);

    private slots:
        void on_itemChanged(int i);
        void closeEditor(QEditor *editor);
        void openEditor();
};

#endif // CLIPBOARDBROWSER_H

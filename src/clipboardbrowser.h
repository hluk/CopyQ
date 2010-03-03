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
#include <QMimeData>
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
        bool add(const QImage &image);
        bool add(const QVariant &value);
        void remove();
        QString itemText(int i = -1) const;
        QString itemText(QModelIndex ind) const;
        QVariant itemData(int row) const {
            return model()->data( index(row), Qt::EditRole );
        }
        void sync(bool list_to_clipboard = true, QClipboard::Mode mode = QClipboard::Clipboard);
        QModelIndex index(int i) const {
            return model()->index(i,0);
        }
        void setCurrent(int row, bool cycle = false, bool selection = false);

        // clipboard monitoring functions
        void setMonitoringInterval(int msec) { m_msec = msec; };
        void startMonitoring();
        void stopMonitoring();
        void checkClipboard();

        // if items selected: return concatenation of selected items
        // else: return text of first item
        const QString selectedText() const;

        // do action on item on given row (default is selected item)
        void openActionDialog(int row = -1, bool modal = true);

    private:
        int m_maxitems;
        int m_msec;
        QString m_editor;
        QBasicTimer timer;
        QBasicTimer timer_save;
        ClipboardModel *m;
        ItemDelegate *d;
        ActionDialog *actionDialog;
        QString m_lastSelection;

        const QString dataFilename() const;

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
        void clipboardChanged(QClipboard::Mode mode = QClipboard::Clipboard);
        void moveToClipboard(const QModelIndex &ind);
        void moveToClipboard(const QString &str);
        void moveToClipboard(int i);
        void filterItems(const QString &str);
        void itemModified(uint hash, const QString &str);
        void addItems(const QStringList &items);
        void closeEditor(QEditor *editor);
        void openEditor();
        void action(int row, const QString &cmd,
                    const QString &sep = QString('\n'),
                    bool input = true, bool output = true);
};

#endif // CLIPBOARDBROWSER_H

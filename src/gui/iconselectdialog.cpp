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

#include "iconselectdialog.h"

#include "common/config.h"
#include "gui/iconfont.h"
#include "gui/icons.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

IconSelectDialog::IconSelectDialog(const QString &defaultIcon, QWidget *parent)
    : QDialog(parent)
    , m_iconList(new QListWidget(this))
    , m_selectedIcon(defaultIcon)
{
    setWindowTitle( tr("CopyQ Select Icon") );

    m_iconList->setViewMode(QListView::IconMode);
    connect( m_iconList, SIGNAL(activated(QModelIndex)),
             this, SLOT(onIconListItemActivated(QModelIndex)) );

    QFontMetrics fm( iconFont() );

    const int gridSize = iconFontSizePixels() + 8;
    const QSize size(gridSize, gridSize);
    m_iconList->setFont( iconFont() );
    m_iconList->setGridSize(size);
    m_iconList->setResizeMode(QListView::Adjust);
    m_iconList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_iconList->setDragDropMode(QAbstractItemView::NoDragDrop);

    m_iconList->addItem( QString("") );
    m_iconList->item(0)->setSizeHint(size);

    for (ushort i = IconFirst; i <= IconLast; ++i) {
        QChar c(i);
        if ( fm.inFont(c) ) {
            const QString icon(c);
            QListWidgetItem *item = new QListWidgetItem(icon, m_iconList);
            item->setSizeHint(size);
            if (defaultIcon == icon)
                m_iconList->setCurrentRow(m_iconList->count() - 1);
        }
    }

    QPushButton *browseButton = new QPushButton(tr("Browse..."), this);
    if ( m_selectedIcon.size() > 2 )
        browseButton->setIcon(QIcon(m_selectedIcon));
    connect( browseButton, SIGNAL(clicked()),
             this, SLOT(onBrowse()) );

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect( buttonBox, SIGNAL(rejected()),
             this, SLOT(reject()) );
    connect( buttonBox, SIGNAL(accepted()),
             this, SLOT(onAcceptCurrent()) );

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_iconList);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    layout->addLayout(buttonLayout);
    buttonLayout->addWidget(browseButton);
    buttonLayout->addWidget(buttonBox);

    m_iconList->setFocus();

    // Restore previous geometry.
    restoreWindowGeometry(this, false);
}

void IconSelectDialog::done(int result)
{
    saveWindowGeometry(this, false);

    if (result == QDialog::Accepted)
        emit iconSelected(m_selectedIcon);

    QDialog::done(result);
}

void IconSelectDialog::onIconListItemActivated(const QModelIndex &index)
{
    m_selectedIcon = m_iconList->item(index.row())->text();
    accept();
}

void IconSelectDialog::onBrowse()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this, tr("Open Icon file"), m_selectedIcon,
                tr("Image Files (*.png *.jpg *.jpeg *.bmp *.ico *.svg)"));
    if ( !fileName.isNull() ) {
        m_selectedIcon = fileName;
        accept();
    }
}

void IconSelectDialog::onAcceptCurrent()
{
    const QModelIndex index = m_iconList->currentIndex();
    if ( index.isValid() && m_iconList->item(index.row())->isSelected() )
        onIconListItemActivated(index);
    else
        reject();
}

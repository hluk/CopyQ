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

#include "itemdata.h"
#include "ui_itemdatasettings.h"

#include <QContextMenuEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QtPlugin>

#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

namespace {

// Limit number of characters for performance reasons.
const int maxChars = 256;

const QStringList defaultFormats = QStringList("text/uri-list") << QString("text/xml");

QString escapeHtml(const QString &str)
{
#if QT_VERSION < 0x050000
    return Qt::escape(str);
#else
    return str.toHtmlEscaped();
#endif
}

} // namespace

ItemData::ItemData(QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
{
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setContentsMargins(4, 4, 4, 4);
    setTextFormat(Qt::RichText);
}

void ItemData::setData(const QModelIndex &index)
{
    QString text;

    const QStringList formats = ItemDataLoader::getFormats(index);
    for (int i = 0; i < formats.size(); ++i ) {
        QString data = ItemDataLoader::getData(i, index).left(maxChars);
        text.append( QString("<p><b>%1</b>: %2</p>")
                     .arg(formats[i])
                     .arg( escapeHtml(data.left(maxChars)) ) );
    }

    setText(text);
}

void ItemData::highlight(const QRegExp &, const QFont &, const QPalette &)
{
}

void ItemData::updateSize()
{
    adjustSize();
}

void ItemData::mousePressEvent(QMouseEvent *e)
{
    QLabel::mousePressEvent(e);
    e->ignore();
}

void ItemData::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( e->modifiers().testFlag(Qt::ShiftModifier) )
        QLabel::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

void ItemData::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

ItemDataLoader::ItemDataLoader()
    : ui(NULL)
{
}

ItemDataLoader::~ItemDataLoader()
{
    delete ui;
}

ItemWidget *ItemDataLoader::create(const QModelIndex &index, QWidget *parent) const
{
    ItemData *item = new ItemData(parent);
    item->setData(index);

    return item;
}

QStringList ItemDataLoader::formatsToSave() const
{
    return m_settings.value("formats", defaultFormats).toStringList();
}

QVariantMap ItemDataLoader::applySettings()
{
    Q_ASSERT(ui != NULL);
    m_settings["formats"] = ui->plainTextEditFormats->toPlainText().split( QRegExp("[;,\\s]+") );
    return  m_settings;
}

QWidget *ItemDataLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemDataSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    QStringList formats = m_settings.value("formats", defaultFormats).toStringList();
    ui->plainTextEditFormats->setPlainText( formats.join(QString("\n")) );

    connect( ui->treeWidgetFormats, SIGNAL(itemActivated(QTreeWidgetItem*,int)),
             SLOT(on_treeWidgetFormats_itemActivated(QTreeWidgetItem*,int)) );

    return w;
}

void ItemDataLoader::on_treeWidgetFormats_itemActivated(QTreeWidgetItem *item, int column)
{
    const QString mime = item->toolTip(column);
    if ( !mime.isEmpty() )
        ui->plainTextEditFormats->appendPlainText(mime);
}

Q_EXPORT_PLUGIN2(itemdata, ItemDataLoader)

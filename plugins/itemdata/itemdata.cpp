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
#include <QTextCodec>
#include <QtPlugin>

#if QT_VERSION < 0x050000
#   include <QTextDocument> // Qt::escape()
#endif

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 256;

const QStringList defaultFormats = QStringList("text/uri-list") << QString("text/xml");

QString escapeHtml(const QByteArray &data)
{
    QTextCodec *codec = QTextCodec::codecForHtml(data, QTextCodec::codecForLocale());
    QString str = codec->toUnicode(data);
#if QT_VERSION < 0x050000
    return Qt::escape(str);
#else
    return str.toHtmlEscaped();
#endif
}

QString hexData(const QByteArray &data)
{
    if ( data.isEmpty() )
        return QString();

    QString result;
    QString chars;

    int i = 0;
    forever {
        if (i > 0) {
            if ( (i % 2) == 0 )
                result.append( QString(" ") );
            if ( (i % 16) == 0 ) {
                result.append(" ");
                result.append(chars);
                result.append( QString("\n") );
                chars.clear();
                if (i >= data.size() )
                    break;
            }
        }
        if ( (i % 16) == 0 ) {
            result.append( QString("%1: ").arg(QString::number(i, 16), 4, QChar('0')) );
        }
        if (i < data.size() ) {
            QChar c = data[i];
            result.append( QString("%1").arg(QString::number(c.unicode(), 16), 2, QChar('0')) );
            chars.append( c.isPrint() ? escapeHtml(QByteArray(1, c.toLatin1())) : QString(".") );
        } else {
            result.append( QString("  ") );
        }

        ++i;
    }

    return result;
}

bool emptyIntersection(const QStringList &lhs, const QStringList &rhs)
{
    for (int i = 0; i < lhs.size(); ++i) {
        if ( rhs.contains(lhs[i]) )
            return false;
    }

    return true;
}

} // namespace

ItemData::ItemData(int maxBytes, QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
    , m_maxBytes(maxBytes)
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
        QByteArray data = ItemDataLoader::getData(i, index);
        const int size = data.size();
        data = data.left(m_maxBytes);
        const QString &format = formats[i];
        bool hasText = format.startsWith("text/") ||
                       format.startsWith("application/x-copyq-owner-window-title");
        text.append( QString("<p>") );
        text.append( QString("<b>%1</b> (%2 bytes)<pre>%3</pre>")
                     .arg(format)
                     .arg(size)
                     .arg(hasText ? escapeHtml(data) : hexData(data)) );
        text.append( QString("</p>") );
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
    if ( emptyIntersection(getFormats(index), formatsToSave()) )
        return NULL;

    ItemData *item = new ItemData( m_settings.value("max_bytes", defaultMaxBytes).toInt(), parent );
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
    m_settings["max_bytes"] = ui->spinBoxMaxChars->value();
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
    ui->spinBoxMaxChars->setValue( m_settings.value("max_bytes", defaultMaxBytes).toInt() );

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

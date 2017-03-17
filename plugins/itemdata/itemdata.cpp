/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "common/textdata.h"

#include <QContextMenuEvent>
#include <QModelIndex>
#include <QMouseEvent>
#include <QTextCodec>
#include <QtPlugin>

namespace {

// Limit number of characters for performance reasons.
const int defaultMaxBytes = 256;

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
            chars.append( c.isPrint() ? escapeHtml(QString(c)) : QString(".") );
        } else {
            result.append( QString("  ") );
        }

        ++i;
    }

    return result;
}

QString stringFromBytes(const QByteArray &bytes, const QString &format)
{
    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    if (format == QLatin1String("text/html"))
        codec = QTextCodec::codecForHtml(bytes, codec);
    return codec->toUnicode(bytes);
}

bool emptyIntersection(const QStringList &lhs, const QStringList &rhs)
{
    for (const auto &l : lhs) {
        if ( rhs.contains(l) )
            return false;
    }

    return true;
}

} // namespace

ItemData::ItemData(const QModelIndex &index, int maxBytes, QWidget *parent)
    : QLabel(parent)
    , ItemWidget(this)
{
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setContentsMargins(4, 4, 4, 4);
    setTextFormat(Qt::RichText);

    QString text;

    const QVariantMap data = index.data(contentType::data).toMap();
    for ( const auto &format : data.keys() ) {
        QByteArray bytes = data[format].toByteArray();
        const int size = bytes.size();
        bool trimmed = size > maxBytes;
        if (trimmed)
            bytes = bytes.left(maxBytes);

        bool hasText = format.startsWith("text/") ||
                       format.startsWith("application/x-copyq-owner-window-title");
        const QString content = hasText ? escapeHtml(stringFromBytes(bytes, format)) : hexData(bytes);
        text.append( QString("<p>") );
        text.append( QString("<b>%1</b> (%2 bytes)<pre>%3</pre>")
                     .arg(format)
                     .arg(size)
                     .arg(content) );
        text.append( QString("</p>") );

        if (trimmed)
            text.append( QString("<p>...</p>") );
    }

    setText(text);
}

void ItemData::highlight(const QRegExp &, const QFont &, const QPalette &)
{
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
{
}

ItemDataLoader::~ItemDataLoader() = default;

ItemWidget *ItemDataLoader::create(const QModelIndex &index, QWidget *parent, bool preview) const
{
    if ( index.data(contentType::isHidden).toBool() )
        return nullptr;

    const QStringList formats = index.data(contentType::data).toMap().keys();
    if ( emptyIntersection(formats, formatsToSave()) )
        return nullptr;

    const int bytes = preview ? 4096 : m_settings.value("max_bytes", defaultMaxBytes).toInt();
    return new ItemData(index, bytes, parent);
}

QStringList ItemDataLoader::formatsToSave() const
{
    return m_settings.contains("formats")
            ? m_settings["formats"].toStringList()
            : QStringList() << mimeUriList << QString("text/xml");
}

QVariantMap ItemDataLoader::applySettings()
{
    Q_ASSERT(ui != nullptr);
    m_settings["formats"] = ui->plainTextEditFormats->toPlainText().split( QRegExp("[;,\\s]+") );
    m_settings["max_bytes"] = ui->spinBoxMaxChars->value();
    return  m_settings;
}

QWidget *ItemDataLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemDataSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    const QStringList formats = formatsToSave();
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

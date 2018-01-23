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

#include "itempinned.h"

#include "common/command.h"
#include "common/contenttype.h"
#include "common/display.h"

#ifdef HAS_TESTS
#   include "tests/itempinnedtests.h"
#endif

#include <QApplication>
#include <QBoxLayout>
#include <QMessageBox>
#include <QModelIndex>

#include <algorithm>

namespace {

const char mimePinned[] = "application/x-copyq-item-pinned";

bool isPinned(const QModelIndex &index)
{
    const auto dataMap = index.data(contentType::data).toMap();
    return dataMap.contains(mimePinned);
}

Command dummyPinCommand()
{
    Command c;
    c.icon = QString(QChar(IconThumbtack));
    c.inMenu = true;
    return c;
}

bool containsPinnedItems(const QModelIndexList &indexList)
{
    return std::any_of( std::begin(indexList), std::end(indexList), isPinned );
}

} // namespace

ItemPinned::ItemPinned(ItemWidget *childItem)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_border(new QWidget(this))
    , m_childItem(childItem)
{
    m_childItem->widget()->setObjectName("item_child");
    m_childItem->widget()->setParent(this);

    m_border->setFixedWidth( pointsToPixels(6) );

    // Set pinned item border color.
    const auto *parent = parentWidget();
    auto color = parent->palette().color(QPalette::Background);
    const int lightThreshold = 100;
    const bool menuBackgrounIsLight = color.lightness() > lightThreshold;
    color.setHsl(
                color.hue(),
                color.saturation(),
                qMax(0, qMin(255, color.lightness() + (menuBackgrounIsLight ? -200 : 50)))
                );
    const auto styleSheet = QString("background-color: rgba(%1,%2,%3,15\\%)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue());
    m_border->setStyleSheet(styleSheet);

    QBoxLayout *layout;
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing( pointsToPixels(5) );

    layout->addWidget(m_childItem->widget());
    layout->addStretch();
    layout->addWidget(m_border);
}

void ItemPinned::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);
}

QWidget *ItemPinned::createEditor(QWidget *parent) const
{
    return m_childItem->createEditor(parent);
}

void ItemPinned::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    m_childItem->setEditorData(editor, index);
}

void ItemPinned::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    m_childItem->setModelData(editor, model, index);
}

bool ItemPinned::hasChanges(QWidget *editor) const
{
    return m_childItem->hasChanges(editor);
}

QObject *ItemPinned::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem->createExternalEditor(index, parent);
}

void ItemPinned::updateSize(QSize maximumSize, int idealWidth)
{
    setMinimumWidth(idealWidth);
    setMaximumWidth(maximumSize.width());
    const int width = m_border->width() + layout()->spacing();
    const int childItemWidth = idealWidth - width;
    const auto childItemMaximumSize = QSize(maximumSize.width() - width, maximumSize.height());
    m_childItem->updateSize(childItemMaximumSize, childItemWidth);
    adjustSize();
}

bool ItemPinnedScriptable::isPinned()
{
    const auto args = currentArguments();
    for (const auto &arg : args) {
        bool ok;
        const int row = arg.toInt(&ok);
        if (ok) {
            const auto result = call("read", QVariantList() << "?" << row);
            if ( result.toByteArray().contains(mimePinned) )
                return true;
        }
    }

    return false;
}

void ItemPinnedScriptable::pin()
{
    const auto args = currentArguments();
    if (args.isEmpty()) {
        pinData();
    } else {
        for (const auto &arg : args) {
            bool ok;
            const int row = arg.toInt(&ok);
            if (ok)
                call("change", QVariantList() << row << mimePinned << QString());
        }
    }
}

void ItemPinnedScriptable::unpin()
{
    const auto args = currentArguments();
    if (args.isEmpty()) {
        unpinData();
    } else {
        for (const auto &arg : args) {
            bool ok;
            const int row = arg.toInt(&ok);
            if (ok)
                call("change", QVariantList() << row << mimePinned << QVariant());
        }
    }
}

void ItemPinnedScriptable::pinData()
{
    call("setData", QVariantList() << mimePinned << QString());
}

void ItemPinnedScriptable::unpinData()
{
    call("removeData", QVariantList() << mimePinned);
}

ItemPinnedSaver::ItemPinnedSaver(QAbstractItemModel *model, QVariantMap &settings, const ItemSaverPtr &saver)
    : m_model(model)
    , m_settings(settings)
    , m_saver(saver)
{
    connect( model, SIGNAL(rowsInserted(QModelIndex,int,int)),
             SLOT(onRowsInserted(QModelIndex,int,int)) );
    connect( model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(onRowsRemoved(QModelIndex,int,int)) );
    connect( model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
             SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );
    connect( model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             SLOT(onDataChanged(QModelIndex,QModelIndex)) );

    updateLastPinned( 0, m_model->rowCount() );
}

bool ItemPinnedSaver::saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file)
{
    return m_saver->saveItems(tabName, model, file);
}

bool ItemPinnedSaver::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    if ( !containsPinnedItems(indexList) )
        return m_saver->canRemoveItems(indexList, error);

    if (error) {
        *error = "Removing pinned item is not allowed (unpin item first)";
        return false;
    }

    QMessageBox::information(
                QApplication::activeWindow(),
                ItemPinnedLoader::tr("Cannot Remove Pinned Items"),
                ItemPinnedLoader::tr("Unpin items first to remove them.") );
    return false;
}

bool ItemPinnedSaver::canMoveItems(const QList<QModelIndex> &indexList)
{
    return !containsPinnedItems(indexList)
            && m_saver->canMoveItems(indexList);
}

void ItemPinnedSaver::itemsRemovedByUser(const QList<QModelIndex> &indexList)
{
    m_saver->itemsRemovedByUser(indexList);
}

QVariantMap ItemPinnedSaver::copyItem(const QAbstractItemModel &model, const QVariantMap &itemData)
{
    return m_saver->copyItem(model, itemData);
}

void ItemPinnedSaver::onRowsInserted(const QModelIndex &, int start, int end)
{
    if (!m_model || m_lastPinned < start) {
        updateLastPinned(start, end);
        return;
    }

    disconnect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                this, SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );

    // Shift rows below inserted up.
    const int rowCount = end - start + 1;
    for (int row = end + 1; row <= m_lastPinned + rowCount; ++row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row - rowCount);
    }

    connect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
             SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );
}

void ItemPinnedSaver::onRowsRemoved(const QModelIndex &, int start, int end)
{
    if (!m_model || m_lastPinned < start)
        return;

    disconnect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                this, SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );

    // Shift rows below removed down.
    const int rowCount = end - start + 1;
    for (int row = m_lastPinned - rowCount; row >= start; --row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row + rowCount + 1);
    }

    connect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
             SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );
}

void ItemPinnedSaver::onRowsMoved(const QModelIndex &, int start, int end, const QModelIndex &, int destinationRow)
{
    if (!m_model)
        return;

    if ( (m_lastPinned >= start || m_lastPinned >= destinationRow)
         && (end >= m_lastPinned || destinationRow >= m_lastPinned) )
    {
        if (start < destinationRow)
            updateLastPinned(start, destinationRow + end - start + 1);
        else
            updateLastPinned(destinationRow, end);
    }

    if (destinationRow != 0 || start < destinationRow)
        return;

    const int rowCount = end - start + 1;

    for (int row = destinationRow; row < destinationRow + rowCount; ++row) {
        if ( isPinned(m_model->index(row, 0)) )
            return;
    }

    disconnect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
                this, SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );

    // Shift rows below inserted up.
    for (int row = destinationRow + rowCount; row <= std::min(m_lastPinned, end); ++row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row - rowCount);
    }

    connect( m_model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
             SLOT(onRowsMoved(QModelIndex,int,int,QModelIndex,int)) );
}

void ItemPinnedSaver::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if ( bottomRight.row() < m_lastPinned )
        return;

    updateLastPinned( topLeft.row(), bottomRight.row() );
}

void ItemPinnedSaver::moveRow(int from, int to)
{
#if QT_VERSION < 0x050000
    QMetaObject::invokeMethod(m_model, "moveRow", Q_ARG(int, from), Q_ARG(int, to));
#else
    m_model->moveRow(QModelIndex(), from, QModelIndex(), to);
#endif
}

void ItemPinnedSaver::updateLastPinned(int from, int to)
{
    for (int row = to; row >= from; --row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) ) {
            m_lastPinned = row;
            break;
        }
    }
}

ItemPinnedLoader::ItemPinnedLoader()
{
}

ItemPinnedLoader::~ItemPinnedLoader() = default;

QStringList ItemPinnedLoader::formatsToSave() const
{
    return QStringList() << mimePinned;
}

QVariantMap ItemPinnedLoader::applySettings()
{
    return m_settings;
}

QWidget *ItemPinnedLoader::createSettingsWidget(QWidget *parent)
{
    return new QWidget(parent);
}

ItemWidget *ItemPinnedLoader::transform(ItemWidget *itemWidget, const QVariantMap &data)
{
    return data.contains(mimePinned) ? new ItemPinned(itemWidget) : nullptr;
}

ItemSaverPtr ItemPinnedLoader::transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model)
{
    return std::make_shared<ItemPinnedSaver>(model, m_settings, saver);
}

QObject *ItemPinnedLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QObject *tests = new ItemPinnedTests(test);
    return tests;
#else
    Q_UNUSED(test);
    return nullptr;
#endif
}

ItemScriptable *ItemPinnedLoader::scriptableObject()
{
    return new ItemPinnedScriptable();
}

QVector<Command> ItemPinnedLoader::commands() const
{
    QVector<Command> commands;

    Command c;

    c = dummyPinCommand();
    c.name = tr("Pin");
    c.input = "!OUTPUT";
    c.output = mimePinned;
    c.cmd = "copyq: plugins.itempinned.pin()";
    commands.append(c);

    c = dummyPinCommand();
    c.name = tr("Unpin");
    c.input = mimePinned;
    c.cmd = "copyq: plugins.itempinned.unpin()";
    commands.append(c);

    return commands;
}

Q_EXPORT_PLUGIN2(itempinned, ItemPinnedLoader)

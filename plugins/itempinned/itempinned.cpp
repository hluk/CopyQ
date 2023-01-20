// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <QPalette>
#include <QPainter>

#include <algorithm>

namespace {

const QLatin1String mimePinned("application/x-copyq-item-pinned");

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
    , ItemWidgetWrapper(childItem, this)
{
    childItem->widget()->setObjectName("item_child");
    childItem->widget()->setParent(this);

    QBoxLayout *layout;
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(childItem->widget());
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

void ItemPinned::paintEvent(QPaintEvent *paintEvent)
{
    const auto *parent = parentWidget();
    auto color = parent->palette().color(QPalette::Window);
    const int lightThreshold = 100;
    const bool menuBackgrounIsLight = color.lightness() > lightThreshold;
    color.setHsl(
                color.hue(),
                color.saturation(),
                qMax(0, qMin(255, color.lightness() + (menuBackgrounIsLight ? -200 : 200)))
                );

    QPainter painter(this);
    const int border = pointsToPixels(6, this);
    const QRect rect(width() - border, 0, width(), height());
    painter.setOpacity(0.15);
    painter.fillRect(rect, color);

    QWidget::paintEvent(paintEvent);
}

void ItemPinned::updateSize(QSize maximumSize, int idealWidth)
{
    setMinimumWidth(idealWidth);
    setMaximumWidth(maximumSize.width());
    const int border = pointsToPixels(12, this);
    const int childItemWidth = idealWidth - border;
    const auto childItemMaximumSize = QSize(maximumSize.width() - border, maximumSize.height());
    ItemWidgetWrapper::updateSize(childItemMaximumSize, childItemWidth);
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
            if ( result.toByteArray().contains(mimePinned.data()) )
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

QString ItemPinnedScriptable::getMimePinned() const
{
    return ::mimePinned;
}

ItemPinnedSaver::ItemPinnedSaver(QAbstractItemModel *model, const ItemSaverPtr &saver)
    : ItemSaverWrapper(saver)
    , m_model(model)
{
    connect( model, &QAbstractItemModel::rowsInserted,
             this, &ItemPinnedSaver::onRowsInserted );
    connect( model, &QAbstractItemModel::rowsRemoved,
             this, &ItemPinnedSaver::onRowsRemoved );
    connect( model, &QAbstractItemModel::rowsMoved,
             this, &ItemPinnedSaver::onRowsMoved );
    connect( model, &QAbstractItemModel::dataChanged,
             this, &ItemPinnedSaver::onDataChanged );

    updateLastPinned( 0, m_model->rowCount() );
}

bool ItemPinnedSaver::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    if ( !containsPinnedItems(indexList) )
        return ItemSaverWrapper::canRemoveItems(indexList, error);

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

bool ItemPinnedSaver::canDropItem(const QModelIndex &index)
{
    return !isPinned(index) && ItemSaverWrapper::canDropItem(index);
}

bool ItemPinnedSaver::canMoveItems(const QList<QModelIndex> &indexList)
{
    return !containsPinnedItems(indexList)
            && ItemSaverWrapper::canMoveItems(indexList);
}

void ItemPinnedSaver::onRowsInserted(const QModelIndex &, int start, int end)
{
    if (!m_model || m_lastPinned < start) {
        updateLastPinned(start, end);
        return;
    }

    disconnect( m_model.data(), &QAbstractItemModel::rowsMoved,
                this, &ItemPinnedSaver::onRowsMoved );

    // Shift rows below inserted up.
    const int rowCount = end - start + 1;
    for (int row = end + 1; row <= m_lastPinned + rowCount; ++row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row - rowCount);
    }

    connect( m_model.data(), &QAbstractItemModel::rowsMoved,
             this, &ItemPinnedSaver::onRowsMoved );
}

void ItemPinnedSaver::onRowsRemoved(const QModelIndex &, int start, int end)
{
    if (!m_model || m_lastPinned < start)
        return;

    disconnect( m_model.data(), &QAbstractItemModel::rowsMoved,
                this, &ItemPinnedSaver::onRowsMoved );

    // Shift rows below removed down.
    const int rowCount = end - start + 1;
    for (int row = m_lastPinned - rowCount; row >= start; --row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row + rowCount + 1);
    }

    connect( m_model.data(), &QAbstractItemModel::rowsMoved,
             this, &ItemPinnedSaver::onRowsMoved );
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

    disconnect( m_model.data(), &QAbstractItemModel::rowsMoved,
                this, &ItemPinnedSaver::onRowsMoved );

    // Shift rows below inserted up.
    for (int row = destinationRow + rowCount; row <= std::min(m_lastPinned, end); ++row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row - rowCount);
    }

    connect( m_model.data(), &QAbstractItemModel::rowsMoved,
             this, &ItemPinnedSaver::onRowsMoved );
}

void ItemPinnedSaver::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if ( bottomRight.row() < m_lastPinned )
        return;

    updateLastPinned( topLeft.row(), bottomRight.row() );
}

void ItemPinnedSaver::moveRow(int from, int to)
{
    m_model->moveRow(QModelIndex(), from, QModelIndex(), to);
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

ItemWidget *ItemPinnedLoader::transform(ItemWidget *itemWidget, const QVariantMap &data)
{
    return data.contains(mimePinned) ? new ItemPinned(itemWidget) : nullptr;
}

ItemSaverPtr ItemPinnedLoader::transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *model)
{
    return std::make_shared<ItemPinnedSaver>(model, saver);
}

QObject *ItemPinnedLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QObject *tests = new ItemPinnedTests(test);
    return tests;
#else
    Q_UNUSED(test)
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
    c.internalId = QStringLiteral("copyq_pinned_pin");
    c.name = tr("Pin");
    c.input = "!OUTPUT";
    c.output = mimePinned;
    c.cmd = "copyq: plugins.itempinned.pin()";
    commands.append(c);

    c = dummyPinCommand();
    c.internalId = QStringLiteral("copyq_pinned_unpin");
    c.name = tr("Unpin");
    c.input = mimePinned;
    c.cmd = "copyq: plugins.itempinned.unpin()";
    commands.append(c);

    return commands;
}

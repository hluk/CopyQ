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
#include "ui_itempinnedsettings.h"

#include "common/common.h"
#include "common/command.h"
#include "common/contenttype.h"

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
    c.icon = QString(QChar(IconThumbTack));
    c.inMenu = true;
    c.shortcuts = QStringList()
            << ItemPinnedLoader::tr("Ctrl+Shift+P", "Shortcut to pin and unpin items");
    return c;
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
    m_border->setStyleSheet("background: rgba(0,0,0,0.15)");

    QBoxLayout *layout;
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing( pointsToPixels(5) );

    layout->addWidget(m_childItem->widget());
    layout->spacerItem();
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
    return m_childItem->setEditorData(editor, index);
}

void ItemPinned::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    return m_childItem->setModelData(editor, model, index);
}

bool ItemPinned::hasChanges(QWidget *editor) const
{
    return m_childItem->hasChanges(editor);
}

QObject *ItemPinned::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem->createExternalEditor(index, parent);
}

void ItemPinned::updateSize(const QSize &maximumSize, int idealWidth)
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
    for (const auto &arg : args) {
        bool ok;
        const int row = arg.toInt(&ok);
        if (ok)
            call("change", QVariantList() << row << mimePinned << QString());
    }
}

void ItemPinnedScriptable::unpin()
{
    const auto args = currentArguments();
    for (const auto &arg : args) {
        bool ok;
        const int row = arg.toInt(&ok);
        if (ok)
            call("change", QVariantList() << row << mimePinned << QVariant());
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
    connect( model, SIGNAL(rowsInserted(QModelIndex, int, int)),
             SLOT(onRowsInserted(QModelIndex, int, int)) );
    connect( model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             SLOT(onRowsRemoved(QModelIndex,int,int)) );
}

bool ItemPinnedSaver::saveItems(const QAbstractItemModel &model, QIODevice *file)
{
    return m_saver->saveItems(model, file);
}

bool ItemPinnedSaver::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    const bool containsPinnedItems = std::any_of(
                std::begin(indexList), std::end(indexList), isPinned);

    if (!containsPinnedItems)
        return m_saver->canRemoveItems(indexList, error);

    if (error) {
        *error = "Removing pinned items is not allowed (plugin.itempinned.unpin() items first)";
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
    return m_saver->canMoveItems(indexList);
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
    if (!m_model)
        return;

    // Shift rows below inserted up.
    const int rowCount = end - start + 1;
    for (int row = end + 1; row < m_model->rowCount(); ++row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row - rowCount);
    }
}

void ItemPinnedSaver::onRowsRemoved(const QModelIndex &, int start, int end)
{
    if (!m_model)
        return;

    // Shift rows below removed down.
    const int rowCount = end - start + 1;
    for (int row = m_model->rowCount() - 1; row >= end; --row) {
        const auto index = m_model->index(row, 0);
        if ( isPinned(index) )
            moveRow(row, row + rowCount + 1);
    }
}

void ItemPinnedSaver::moveRow(int from, int to)
{
#if QT_VERSION < 0x050000
    QMetaObject::invokeMethod(m_model, "moveRow", Q_ARG(int, from), Q_ARG(int, to));
#else
    m_model->moveRow(QModelIndex(), from, QModelIndex(), to);
#endif
}

ItemPinnedLoader::ItemPinnedLoader()
{
}

ItemPinnedLoader::~ItemPinnedLoader()
{
}

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
    ui.reset(new Ui::ItemPinnedSettings);
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    connect( ui->pushButtonAddCommands, SIGNAL(clicked()),
             this, SLOT(addCommands()) );

    return w;
}

ItemWidget *ItemPinnedLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    return isPinned(index) ? new ItemPinned(itemWidget) : nullptr;
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

ItemScriptable *ItemPinnedLoader::scriptableObject(QObject *parent)
{
    return new ItemPinnedScriptable(parent);
}

QList<Command> ItemPinnedLoader::commands() const
{
    QList<Command> commands;

    Command c;

    c = dummyPinCommand();
    c.name = tr("Pin");
    c.input = "!OUTPUT";
    c.output = mimePinned;
    c.cmd = "copyq: plugins.itempinned.pinData()";
    commands.append(c);

    c = dummyPinCommand();
    c.name = tr("Unpin");
    c.input = mimePinned;
    c.cmd = "copyq: plugins.itempinned.unpinData()";
    commands.append(c);

    return commands;
}

void ItemPinnedLoader::addCommands()
{
    emit addCommands(commands());
}

Q_EXPORT_PLUGIN2(itempinned, ItemPinnedLoader)

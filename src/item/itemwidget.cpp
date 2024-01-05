// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemwidget.h"

#include "common/command.h"
#include "common/contenttype.h"
#include "common/mimetypes.h"
#include "item/itemeditor.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QMimeData>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QTextEdit>
#include <QUrl>
#include <QWidget>

namespace {

bool canMouseInteract(const QMouseEvent &event)
{
    return event.modifiers() & Qt::ShiftModifier;
}

} // namespace

ItemWidget::ItemWidget(QWidget *widget)
    : m_widget(widget)
{
    Q_ASSERT(widget != nullptr);

    // Object name for style sheet.
    widget->setObjectName("item");

    // Item widgets are not focusable.
    widget->setFocusPolicy(Qt::NoFocus);

    // Limit size of items.
    widget->setMaximumSize(2048, 2048);

    // Disable drag'n'drop by default.
    widget->setAcceptDrops(false);
}

void ItemWidget::updateSize(QSize maximumSize, int idealWidth)
{
    QWidget *w = widget();
    w->setMaximumSize(maximumSize);
    const int idealHeight = w->heightForWidth(idealWidth);
    const int maximumHeight = w->heightForWidth(maximumSize.width());
    if (idealHeight <= 0 && maximumHeight <= 0)
        w->resize(w->sizeHint());
    else if (idealHeight != maximumHeight)
        w->setFixedSize( maximumSize.width(), maximumHeight );
    else
        w->setFixedSize(idealWidth, idealHeight);
}

void ItemWidget::setCurrent(bool current)
{
    // Propagate mouse events to item list until the item is selected.
    widget()->setAttribute(Qt::WA_TransparentForMouseEvents, !current);
}

bool ItemWidget::filterMouseEvents(QTextEdit *edit, QEvent *event)
{
    const auto type = event->type();

    bool allowMouseInteraction = true;

    switch (type) {
    case QEvent::Enter:
        edit->setMouseTracking(true);
        edit->viewport()->setCursor( QCursor() );
        return false;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( !canMouseInteract(*e) )
            allowMouseInteraction = false;
        else if (e->button() == Qt::LeftButton)
            edit->setTextCursor( edit->cursorForPosition(e->pos()) );

        break;
    }

    case QEvent::MouseMove: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( !canMouseInteract(*e) )
            allowMouseInteraction = false;

        break;
    }

    case QEvent::MouseButtonRelease: {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);

        if ( canMouseInteract(*e) && edit->textCursor().hasSelection() )
            edit->copy();

        allowMouseInteraction = false;

        break;
    }

    default:
        return false;
    }

    Qt::TextInteractionFlags flags = edit->textInteractionFlags();
    if (allowMouseInteraction) {
        flags |= Qt::TextSelectableByMouse;
        flags |= Qt::LinksAccessibleByMouse;
    } else {
        flags &= ~Qt::TextSelectableByMouse;
        flags &= ~Qt::LinksAccessibleByMouse;
    }
    edit->setTextInteractionFlags(flags);

    if (type == QEvent::MouseButtonPress || type == QEvent::MouseMove) {
        const auto e = static_cast<QMouseEvent*>(event);
        if (allowMouseInteraction) {
            const auto anchor = edit->anchorAt(e->pos());
            if ( anchor.isEmpty() ) {
                edit->viewport()->setCursor( QCursor(Qt::IBeamCursor) );
            } else {
                edit->viewport()->setCursor( QCursor(Qt::PointingHandCursor) );
                if (type == QEvent::MouseButtonPress) {
                    QDesktopServices::openUrl( QUrl(anchor) );
                    e->accept();
                    return true;
                }
            }
        } else {
            edit->viewport()->setCursor( QCursor() );
        }
    }

    return false;
}

QVariant ItemScriptable::call(const QString &method, const QVariantList &arguments)
{
    QVariant result;
    QMetaObject::invokeMethod(
                m_scriptable, "call", Qt::DirectConnection,
                Q_RETURN_ARG(QVariant, result),
                Q_ARG(QString, method),
                Q_ARG(QVariantList, arguments));
    return result;
}

QVariant ItemScriptable::eval(const QString &script)
{
    return call("eval", QVariantList() << script);
}

QVariantList ItemScriptable::currentArguments()
{
    QVariantList arguments;
    QMetaObject::invokeMethod(
                m_scriptable, "currentArguments", Qt::DirectConnection,
                Q_RETURN_ARG(QVariantList, arguments) );
    return arguments;
}

void ItemScriptable::throwError(const QString &message)
{
    QMetaObject::invokeMethod(
                m_scriptable, "throwException", Qt::DirectConnection,
                Q_ARG(QString, message) );
}

bool ItemSaverInterface::saveItems(const QString &, const QAbstractItemModel &, QIODevice *)
{
    return false;
}

bool ItemSaverInterface::canRemoveItems(const QList<QModelIndex> &, QString *)
{
    return true;
}

bool ItemSaverInterface::canDropItem(const QModelIndex &)
{
    return true;
}

bool ItemSaverInterface::canMoveItems(const QList<QModelIndex> &)
{
    return true;
}

void ItemSaverInterface::itemsRemovedByUser(const QList<QPersistentModelIndex> &)
{
}

QVariantMap ItemSaverInterface::copyItem(const QAbstractItemModel &, const QVariantMap &itemData)
{
    return itemData;
}

void ItemSaverInterface::setFocus(bool)
{
}

ItemWidget *ItemLoaderInterface::create(const QVariantMap &, QWidget *, bool) const
{
    return nullptr;
}

QStringList ItemLoaderInterface::formatsToSave() const
{
    return {};
}

bool ItemLoaderInterface::canLoadItems(QIODevice *) const
{
    return false;
}

bool ItemLoaderInterface::canSaveItems(const QString &) const
{
    return false;
}

ItemSaverPtr ItemLoaderInterface::loadItems(const QString &, QAbstractItemModel *, QIODevice *, int)
{
    return nullptr;
}

ItemSaverPtr ItemLoaderInterface::initializeTab(const QString &, QAbstractItemModel *, int)
{
    return nullptr;
}

ItemWidget *ItemLoaderInterface::transform(ItemWidget *, const QVariantMap &)
{
    return nullptr;
}

ItemSaverPtr ItemLoaderInterface::transformSaver(const ItemSaverPtr &saver, QAbstractItemModel *)
{
    return saver;
}

bool ItemLoaderInterface::matches(const QModelIndex &, const ItemFilter &) const
{
    return false;
}

QObject *ItemLoaderInterface::tests(const TestInterfacePtr &) const
{
    return nullptr;
}

const QObject *ItemLoaderInterface::signaler() const
{
    return nullptr;
}

ItemScriptable *ItemLoaderInterface::scriptableObject()
{
    return nullptr;
}

QVector<Command> ItemLoaderInterface::commands() const
{
    return QVector<Command>();
}

bool ItemLoaderInterface::data(QVariantMap *, const QModelIndex &) const
{
    return true;
}

bool ItemLoaderInterface::setData(const QVariantMap &, const QModelIndex &, QAbstractItemModel *) const
{
    return false;
}

QObject *ItemLoaderInterface::createExternalEditor(const QModelIndex &, const QVariantMap &, QWidget *) const
{
    return nullptr;
}


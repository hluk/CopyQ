#include "itemwidgetwrapper.h"

#include <QSize>

ItemWidgetWrapper::ItemWidgetWrapper(ItemWidget *childItem, QWidget *widget)
    : ItemWidget(widget)
    , m_childItem(childItem)
{
}

void ItemWidgetWrapper::updateSize(QSize maximumSize, int idealWidth)
{
    childItem()->updateSize(maximumSize, idealWidth);
}

void ItemWidgetWrapper::setCurrent(bool current)
{
    childItem()->setCurrent(current);
}

void ItemWidgetWrapper::setTagged(bool tagged)
{
    childItem()->setTagged(tagged);
}

void ItemWidgetWrapper::highlight(const QRegularExpression &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    childItem()->setHighlight(re, highlightFont, highlightPalette);
}

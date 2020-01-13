#ifndef ITEMWIDGETWRAPPER_H
#define ITEMWIDGETWRAPPER_H

#include "itemwidget.h"

#include <memory>

class ItemWidgetWrapper : public ItemWidget
{
public:
    ItemWidgetWrapper(ItemWidget *childItem, QWidget *widget);

    void updateSize(QSize maximumSize, int idealWidth) override;

    void setCurrent(bool current) override;

    void setTagged(bool tagged) override;

protected:
    void highlight(const QRegularExpression &re, const QFont &highlightFont,
                   const QPalette &highlightPalette) override;

    ItemWidget *childItem() const { return m_childItem.get(); }

private:
    std::unique_ptr<ItemWidget> m_childItem;
};

#endif // ITEMWIDGETWRAPPER_H

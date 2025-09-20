#pragma once


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
    ItemWidget *childItem() const { return m_childItem.get(); }

private:
    std::unique_ptr<ItemWidget> m_childItem;
};

#pragma once


#include <QPaintDevice>

static qreal pixelRatio(QPaintDevice *pd)
{
    return pd->devicePixelRatioF();
}

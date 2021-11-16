#ifndef PIXEL_RATIO_H
#define PIXEL_RATIO_H

#include <QPaintDevice>

static qreal pixelRatio(QPaintDevice *pd)
{
    return pd->devicePixelRatioF();
}

#endif // PIXEL_RATIO_H

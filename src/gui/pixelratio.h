#ifndef PIXEL_RATIO_H
#define PIXEL_RATIO_H

#include <QPaintDevice>

static qreal pixelRatio(QPaintDevice *pd)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
    return pd->devicePixelRatioF();
#else
    return static_cast<qreal>(pd->devicePixelRatio());
#endif
}

#endif // PIXEL_RATIO_H

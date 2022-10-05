// SPDX-License-Identifier: GPL-3.0-or-later

#include "platformcommon.h"
#include "platformwindow.h"

#include "common/log.h"
#include "common/appconfig.h"

#include <QEventLoop>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

namespace {

const QLatin1String optionName("paste_with_ctrl_v_windows");

} // namespace

bool pasteWithCtrlV(PlatformWindow &window, const AppConfig &config)
{
    const auto value = config.option<Config::window_paste_with_ctrl_v_regex>();
    const auto pattern = value.isEmpty() ? QSettings().value(optionName).toString() : value;
    if (pattern.isEmpty())
        return false;

    const QRegularExpression re(pattern);
    if (!re.isValid()) {
        log(QString("Invalid regular expression in option \"%1\": %2")
            .arg(optionName, re.errorString()), LogWarning);
        return false;
    }

    const QString windowTitle = window.getTitle();

    if ( !windowTitle.contains(re) ) {
        COPYQ_LOG(QString("Paste with standard shortcut to window \"%1\".")
                  .arg(windowTitle));
        return false;
    }

    COPYQ_LOG(QString("Paste with Ctrl+V requested with option \"%1\" for window \"%2\".")
              .arg(optionName, windowTitle));
    return true;
}

void waitMs(int msec)
{
    if (msec <= 0)
        return;

    QEventLoop loop;
    QTimer t;
    QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(msec);
    loop.exec();
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/diagnostics.h"

#include "common/audioplayer.h"
#include "common/config.h"
#include "common/log.h"
#include "common/version.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QSysInfo>

#ifdef WITH_NATIVE_NOTIFICATIONS
#   include <knotifications_version.h>
#   include <kstatusnotifieritem_version.h>
#endif
#ifdef HAS_KGUIADDONS
#   include <kguiaddons_version.h>
#endif
#ifdef WITH_QCA_ENCRYPTION
#   include <qca_version.h>
#endif

namespace {

QString pluginsPath()
{
#ifdef COPYQ_PLUGIN_PREFIX
    return adjustedInstallPath(QStringLiteral(COPYQ_PLUGIN_PREFIX));
#else
    QDir dir;
    if (platformNativeInterface()->findPluginDir(&dir))
        return dir.absolutePath();
    return QString();
#endif
}

QString themesPath()
{
#ifdef COPYQ_THEME_PREFIX
    return adjustedInstallPath(QStringLiteral(COPYQ_THEME_PREFIX));
#else
    return platformNativeInterface()->themePrefix();
#endif
}

QString translationsPath()
{
#ifdef COPYQ_TRANSLATION_PREFIX
    return adjustedInstallPath(QStringLiteral(COPYQ_TRANSLATION_PREFIX));
#else
    return platformNativeInterface()->translationPrefix();
#endif
}

QString compilerName()
{
#if defined(Q_CC_GNU)
    return QStringLiteral("GCC");
#elif defined(Q_CC_CLANG)
    return QStringLiteral("Clang");
#elif defined(Q_CC_MSVC)
    return QStringLiteral("MSVC");
#else
    return QStringLiteral("???");
#endif
}
std::pair<QString, QString> envEntry(const char *name)
{
    return {QString::fromLatin1(name), qEnvironmentVariable(name)};
}

} // namespace

QMap<QString, QString> pathMap()
{
    return {
        {"config", QSettings().fileName()},
        {"data", itemDataPath()},
#ifdef COPYQ_DESKTOP_FILE
        {"desktop", QStringLiteral(COPYQ_DESKTOP_FILE)},
#endif
        {"exe", QCoreApplication::applicationFilePath()},
#ifdef COPYQ_ICON_PREFIX
        {"icons", QStringLiteral(COPYQ_ICON_PREFIX)},
#endif
        {"log", logFileName()},
        {"plugins", pluginsPath()},
        {"themes", themesPath()},
        {"themes(custom)", qEnvironmentVariable("COPYQ_THEME_PREFIX")},
        {"translations", translationsPath()},
        {"translations(custom)", qEnvironmentVariable("COPYQ_TRANSLATION_PREFIX")},
    };
}

QMap<QString, QString> infoMap()
{
    return {
        {"Arch", QSysInfo::buildAbi()},
        {"Audio", audioBackendVersion()},
        {"Compiler", compilerName()},
#ifdef WITH_NATIVE_NOTIFICATIONS
        {"KNotifications", KNOTIFICATIONS_VERSION_STRING},
#   ifdef KSTATUSNOTIFIERITEM_VERSION_STRING
        {"KStatusNotifierItem", KSTATUSNOTIFIERITEM_VERSION_STRING},
#   endif
#endif
#ifdef HAS_KGUIADDONS
        {"KGuiAddons", KGUIADDONS_VERSION_STRING},
#endif
        {"OS", QSysInfo::prettyProductName()},
#ifdef WITH_QCA_ENCRYPTION
        {"QCA", QCA_VERSION_STR},
#endif
        {"Qt", QT_VERSION_STR},
#ifdef WITH_KEYCHAIN
        {"QtKeychain", QTKEYCHAIN_VERSION_STRING},
#endif
        {"has-audio",
#ifdef WITH_AUDIO
            "1"
#else
            "0"
#endif
        },
        {"has-encryption",
#ifdef WITH_QCA_ENCRYPTION
            "1"
#else
            "0"
#endif
        },
        {"has-global-shortcuts",
#ifdef COPYQ_GLOBAL_SHORTCUTS
            "1"
#else
            "0"
#endif
        },
        {"has-keychain",
#ifdef WITH_KEYCHAIN
            "1"
#else
            "0"
#endif
        },
        {"has-mouse-selection",
#ifdef HAS_MOUSE_SELECTIONS
            "1"
#else
            "0"
#endif
        },
#ifdef Q_OS_LINUX
        envEntry("DISPLAY"),
        envEntry("WAYLAND_DISPLAY"),
        envEntry("XDG_CURRENT_DESKTOP"),
        envEntry("XDG_SESSION_DESKTOP"),
        envEntry("XDG_SESSION_TYPE"),
#endif
    };
}

QString diagnosticText()
{
    QString result;
    const auto info = infoMap();
    for (auto it = info.constBegin(); it != info.constEnd(); ++it) {
        if (!it.value().isEmpty())
            result.append(QStringLiteral("%1: %2\n").arg(it.key(), it.value()));
    }
    result.chop(1);
    return result;
}

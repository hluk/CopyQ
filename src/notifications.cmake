OPTION(WITH_NATIVE_NOTIFICATIONS "Build with native notification support" ON)

if (WITH_NATIVE_NOTIFICATIONS AND WITH_QT6)
    message(WARNING "Native notifications are not supported with Qt 6")
elseif (WITH_NATIVE_NOTIFICATIONS)
    set(KF5_MIN_VERSION "5.18.0")

    find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Notifications)

    list(APPEND copyq_LIBRARIES KF5::Notifications)

    list(APPEND copyq_SOURCES
        gui/notificationnative/notificationnative.cpp
        gui/notificationnative/notificationdaemonnative.cpp)

    list(APPEND copyq_DEFINITIONS WITH_NATIVE_NOTIFICATIONS)
endif()

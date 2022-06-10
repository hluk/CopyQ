OPTION(WITH_NATIVE_NOTIFICATIONS "Build with native notification support" ON)

if (WITH_NATIVE_NOTIFICATIONS)
    set(KF5_MIN_VERSION "5.18.0")

    find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    if (WITH_QT6)
        set(KF6_MIN_VERSION "5.248.0")
        find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Notifications StatusNotifierItem)
        list(APPEND copyq_LIBRARIES KF6::Notifications KF6::StatusNotifierItem)
    else()
        find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Notifications)
        list(APPEND copyq_LIBRARIES KF5::Notifications)
    endif()

    list(APPEND copyq_SOURCES
        gui/notificationnative/notificationnative.cpp
        gui/notificationnative/notificationdaemonnative.cpp)

    list(APPEND copyq_DEFINITIONS WITH_NATIVE_NOTIFICATIONS)
endif()

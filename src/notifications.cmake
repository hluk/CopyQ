OPTION(WITH_NATIVE_NOTIFICATIONS "Build with native notification support" ON)

if (WITH_NATIVE_NOTIFICATIONS)
    find_package(ECM REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    set(KF6_MIN_VERSION "5.248.0")
    find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Notifications StatusNotifierItem)
    list(APPEND copyq_LIBRARIES KF6::Notifications KF6::StatusNotifierItem)
    list(APPEND copyq_SOURCES gui/notificationnative/notificationnative.cpp)
    list(APPEND copyq_DEFINITIONS WITH_NATIVE_NOTIFICATIONS)
endif()

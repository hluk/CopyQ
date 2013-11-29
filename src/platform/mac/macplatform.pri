LIBS    += -framework Carbon -framework Cocoa
SOURCES += \
           ../qxt/qxtglobalshortcut_mac.cpp
USE_QXT = 1

OBJECTIVE_SOURCES += \
    platform/mac/macplatform.mm \
    platform/mac/macactivity.mm

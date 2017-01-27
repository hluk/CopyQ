CONFIG += c++11

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    QMAKE_MAC_SDK = macosx # work around QTBUG-41238

    # Only Intel binaries are accepted so force this
    CONFIG += x86
}

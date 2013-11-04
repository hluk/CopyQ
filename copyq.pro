include("./common.pri")

TEMPLATE = subdirs

# generate cache file for build
cache()

DEFINES += QT_NO_CAST_TO_ASCII
SUBDIRS += src \
           plugins
TRANSLATIONS = \
    translations/copyq_ar.ts \
    translations/copyq_cs.ts \
    translations/copyq_da.ts \
    translations/copyq_de.ts \
    translations/copyq_es.ts \
    translations/copyq_fr.ts \
    translations/copyq_hu.ts \
    translations/copyq_it.ts \
    translations/copyq_ja.ts \
    translations/copyq_lt.ts \
    translations/copyq_nb.ts \
    translations/copyq_nl.ts \
    translations/copyq_pl.ts \
    translations/copyq_pt_PT.ts \
    translations/copyq_pt_BR.ts \
    translations/copyq_ru.ts \
    translations/copyq_sk.ts \
    translations/copyq_sv.ts \
    translations/copyq_tr.ts \
    translations/copyq_uk.ts \
    translations/copyq_zh_CN.ts \
    translations/copyq_zh_TW.ts

macx {
    # Package the CopyQ plugins into the app bundle
    package_plugins.commands = \
        mkdir -p copyq.app/Contents/PlugIns/copyq/ ; \
        cp plugins/*.dylib copyq.app/Contents/PlugIns/copyq/
    package_plugins.depends = sub-plugins sub-src
    QMAKE_EXTRA_TARGETS += package_plugins

    # Package the Qt frameworks into the app bundle
    package_frameworks.commands = \
        test -e copyq.app/Contents/Frameworks/QtCore.framework \
        || $$dirname(QMAKE_QMAKE)/macdeployqt copyq.app
    package_frameworks.target = copyq.app/Contents/Frameworks/QtCore.framework
    package_frameworks.depends = sub-src sub-plugins package_plugins
    QMAKE_EXTRA_TARGETS += package_frameworks

    # Package the translations
    package_translations.commands = \
        $$dirname(QMAKE_QMAKE)/lrelease $$_PRO_FILE_PWD_/copyq.pro && \
        mkdir -p copyq.app/Contents/Resources/translations && \
        cp $$_PRO_FILE_PWD_/translations/*.qm copyq.app/Contents/Resources/translations
    QMAKE_EXTRA_TARGETS += package_translations

    # Rename to CopyQ.app to make it look better
    bundle_mac.depends = package_frameworks package_plugins package_translations
    bundle_mac.target = CopyQ.app
    bundle_mac.commands = mv copyq.app CopyQ.app
    QMAKE_EXTRA_TARGETS += bundle_mac
}

OTHER_FILES += \
    android/AndroidManifest.xml \
    android/version.xml \
    android/res/values-rs/strings.xml \
    android/res/values-zh-rTW/strings.xml \
    android/res/values-ro/strings.xml \
    android/res/values-de/strings.xml \
    android/res/values-it/strings.xml \
    android/res/values-pt-rBR/strings.xml \
    android/res/values-pl/strings.xml \
    android/res/values-ms/strings.xml \
    android/res/values-es/strings.xml \
    android/res/values-id/strings.xml \
    android/res/values/libs.xml \
    android/res/values/strings.xml \
    android/res/values-ja/strings.xml \
    android/res/values-fr/strings.xml \
    android/res/layout/splash.xml \
    android/res/values-et/strings.xml \
    android/res/values-zh-rCN/strings.xml \
    android/res/values-el/strings.xml \
    android/res/values-ru/strings.xml \
    android/res/values-fa/strings.xml \
    android/res/values-nb/strings.xml \
    android/res/values-nl/strings.xml \
    android/src/org/kde/necessitas/ministro/IMinistro.aidl \
    android/src/org/kde/necessitas/ministro/IMinistroCallback.aidl \
    android/src/org/qtproject/qt5/android/bindings/QtActivity.java \
    android/src/org/qtproject/qt5/android/bindings/QtApplication.java

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

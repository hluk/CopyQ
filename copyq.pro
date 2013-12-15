TEMPLATE = subdirs

# generate cache file for build
cache()

SUBDIRS += src \
           plugins
TRANSLATIONS = translations/copyq_cs.ts \
               translations/copyq_de.ts \
               translations/copyq_es.ts

macx {
    package_frameworks.commands = \
        test -e copyq.app/Contents/Frameworks/QtCore.framework \
        || $$dirname(QMAKE_QMAKE)/macdeployqt copyq.app
    package_frameworks.target = copyq.app/Contents/Frameworks/QtCore.framework
    package_frameworks.depends = sub-src sub-plugins
    QMAKE_EXTRA_TARGETS += package_frameworks

    package_plugins.commands = \
        mkdir -p copyq.app/Contents/Resources/plugins/ ; \
        cp plugins/*.dylib copyq.app/Contents/Resources/plugins/
    package_plugins.depends = sub-plugins
    QMAKE_EXTRA_TARGETS += package_plugins

    bundle_mac.depends = package_frameworks package_plugins
    bundle_mac.target = copyq.app
    QMAKE_EXTRA_TARGETS += bundle_mac
}

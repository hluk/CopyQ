TEMPLATE = subdirs

# generate cache file for build
cache()

SUBDIRS += src \
           plugins
TRANSLATIONS = translations/copyq_cs.ts \
               translations/copyq_de.ts \
               translations/copyq_es.ts

macx {
    add_frameworks.commands = \
        test -e copyq.app/Contents/Frameworks/QtCore.framework \
        || $$dirname(QMAKE_QMAKE)/macdeployqt copyq.app
    add_frameworks.target = copyq.app/Contents/Frameworks/QtCore.framework
    add_frameworks.depends = sub-src sub-plugins
    QMAKE_EXTRA_TARGETS += add_frameworks

    bundle_mac.depends = add_frameworks
    bundle_mac.target = copyq.app
    QMAKE_EXTRA_TARGETS += bundle_mac
}

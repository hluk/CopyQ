
include("./common.pri")
TEMPLATE = subdirs

# generate cache file for build
cache()

DEFINES += QT_NO_CAST_TO_ASCII
SUBDIRS += src \
           plugins
TRANSLATIONS = \
    translations/copyq_cs.ts \
    translations/copyq_da.ts \
    translations/copyq_de.ts \
    translations/copyq_es.ts \
    translations/copyq_fr.ts \
    translations/copyq_hu.ts \
    translations/copyq_it.ts \
    translations/copyq_ja.ts \
    translations/copyq_nl.ts \
    translations/copyq_pl.ts \
    translations/copyq_pt_PT.ts \
    translations/copyq_pt_BR.ts \
    translations/copyq_ru.ts \
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

    # Create a dmg file
    dmg.commands = $$_PRO_FILE_PWD_/utils/create_dmg.sh
    dmg.depends = bundle_mac
    QMAKE_EXTRA_TARGETS += dmg
}

# Project files
file(GLOB copyq_SOURCES
    app/*.cpp
    common/*.cpp
    gui/*.cpp
    gui/notification.h
    item/*.cpp
    scriptable/*.cpp
    scriptable/scriptableproxy.h
    )

file(GLOB copyq_FORMS
    ui/*.ui
    )

# translations
file(GLOB copyq_TRANSLATIONS ../translations/*.ts)

set(copyq_RESOURCES copyq.qrc)

# Qt include paths and definitions
include(notifications.cmake)
if (WITH_AUDIO)
    include(audio.cmake)
endif()
include(platform/platform.cmake)

set_source_files_properties(
    main.cpp ${copyq_SOURCES} PROPERTIES
    COMPILE_OPTIONS "${copyq_pedantic_flags}")

# Qxt - global shortcuts
if (USE_QXT)
    list(APPEND copyq_SOURCES ../qxt/qxtglobalshortcut.cpp)
    list(APPEND copyq_DEFINITIONS QXT_STATIC COPYQ_GLOBAL_SHORTCUTS)
endif()

qt_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
qt_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

# Generate version.h from current git commit if available.
set(copyq_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/common/version.cpp")
add_custom_target(
    generate_version_file
    ALL
    COMMAND ${CMAKE_COMMAND}
        -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/common/version.cpp.in
        -D OUTPUT_FILE=${copyq_VERSION_FILE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/version_file.cmake
    BYPRODUCTS ${copyq_VERSION_FILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_source_files_properties(${copyq_VERSION_FILE} PROPERTIES GENERATED TRUE)
set_property(SOURCE ${copyq_VERSION_FILE} PROPERTY SKIP_AUTOMOC ON)

set(copyq_COMPILE
    ${copyq_COMPILE}
    ${copyq_SOURCES}
    ${copyq_FORMS_HEADERS}
    ${copyq_VERSION_FILE}
   )
add_library(copyq-common OBJECT ${copyq_COMPILE})

find_package(${copyq_qt} REQUIRED COMPONENTS LinguistTools)
if("${QT_DEFAULT_MAJOR_VERSION}" STREQUAL "5")
    qt5_add_translation(copyq_QM ${copyq_TRANSLATIONS})
else()
    qt_add_translations(copyq-common
        TS_FILES ${copyq_TRANSLATIONS}
        QM_FILES_OUTPUT_VARIABLE copyq_QM
    )
endif()

if (NOT copyq_version)
    message(FATAL_ERROR "Application version is unset")
endif()

if (NOT APPLE)
    add_executable(${COPYQ_EXECUTABLE_NAME}
        ${copyq_windows_no_console}
        main.cpp
        ${copyq_RESOURCES_RCC}
        ${copyq_QM}
        $<TARGET_OBJECTS:copyq-common>
        ${MINIAUDIO_OBJECTS}
    )
else()
    FIND_LIBRARY(CARBON_LIBRARY Carbon REQUIRED)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa REQUIRED)
    MARK_AS_ADVANCED(CARBON_LIBRARY COCOA_LIBRARY)
    list(APPEND copyq_LIBRARIES "${CARBON_LIBRARY}" "${COCOA_LIBRARY}")

    set(COPYQ_VERSION_REGEX "^([0-9]+)\.([0-9]+)\.(.*)$")
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\1" CPACK_PACKAGE_VERSION_MAJOR
        "${copyq_version}"
        )
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\2" CPACK_PACKAGE_VERSION_MINOR
        "${copyq_version}"
        )
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\3" CPACK_PACKAGE_VERSION_PATCH
        "${copyq_version}"
        )

    set(MACOSX_BUNDLE_BUNDLE_NAME "${COPYQ_EXECUTABLE_NAME}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.hluk.CopyQ")
    set(MACOSX_BUNDLE_INFO_STRING "Advanced clipboard manager with editing and scripting features")
    set(MACOSX_BUNDLE_ICON_FILE "icon.icns")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${copyq_version}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${copyq_version}")

    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/images/icon.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set(copyq_COMPILE ${copyq_COMPILE} "${CMAKE_CURRENT_SOURCE_DIR}/images/icon.icns")

    add_executable(${COPYQ_EXECUTABLE_NAME} MACOSX_BUNDLE
        main.cpp
        ${copyq_RESOURCES_RCC}
        ${copyq_QM}
        $<TARGET_OBJECTS:copyq-common>
        ${MINIAUDIO_OBJECTS}
        "${CMAKE_CURRENT_SOURCE_DIR}/images/icon.icns"
    )
endif()

target_sources(copyq-common PRIVATE ${copyq_SOURCES})
add_dependencies(copyq-common generate_version_file)

# Qt modules
list(APPEND copyq_qt_modules Network Svg Xml Qml)
foreach (copyq_qt_module ${copyq_qt_modules})
    find_package(${copyq_qt}${copyq_qt_module} REQUIRED)
    list(APPEND copyq_LIBRARIES ${copyq_qt}::${copyq_qt_module})
endforeach()

set_target_properties(copyq-common PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")
set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "${copyq_LINK_FLAGS}")
target_link_libraries(copyq-common ${copyq_LIBRARIES})
target_link_libraries(${COPYQ_EXECUTABLE_NAME} ${copyq_LIBRARIES})
target_include_directories(copyq-common PRIVATE ${CMAKE_CURRENT_BINARY_DIR} .)
if (WITH_AUDIO)
    # SYSTEM suppresses warnings from the third-party header on Clang/GCC.
    if (MSVC)
        target_include_directories(copyq-common PRIVATE "${MINIAUDIO_INCLUDE_DIR}")
    else()
        target_include_directories(copyq-common SYSTEM PRIVATE "${MINIAUDIO_INCLUDE_DIR}")
    endif()
endif()
target_include_directories(${COPYQ_EXECUTABLE_NAME} PRIVATE .)

# Precompiled headers for faster MSVC builds.
# Clang with -Weverything and ccache+PCH cause issues on other platforms.
if (MSVC)
    target_precompile_headers(copyq-common PRIVATE
        <QApplication>
        <QByteArray>
        <QCoreApplication>
        <QDir>
        <QFile>
        <QList>
        <QLoggingCategory>
        <QMimeData>
        <QObject>
        <QRegularExpression>
        <QSettings>
        <QString>
        <QStringList>
        <QTimer>
        <QVariant>
        <QVariantMap>
        <QWidget>
    )
endif()

# install
if (WIN32)
    install(TARGETS ${COPYQ_EXECUTABLE_NAME}
        RUNTIME DESTINATION . COMPONENT Runtime
        LIBRARY DESTINATION . COMPONENT Runtime
        )
else()
    install(TARGETS ${COPYQ_EXECUTABLE_NAME}
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        )
endif()

if (NOT APPLE)
    if (TRANSLATION_INSTALL_PREFIX)
        install(FILES ${copyq_QM} DESTINATION "${TRANSLATION_INSTALL_PREFIX}")
    endif()
else()
    set_source_files_properties(${copyq_QM} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/translations")

    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../shared/Info.plist")
    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks")

    set(BUNDLE "\${CMAKE_INSTALL_PREFIX}/${COPYQ_EXECUTABLE_NAME}.app")

    # https://doc.qt.io/qt-6/qt-deploy-runtime-dependencies.html
    # Generate a deployment script to be executed at install time.
    # Collect third-party library directories from CMAKE_PREFIX_PATH so
    # macdeployqt can find them (via -libpath) and resolve their Qt deps.
    set(COPYQ_DEPLOY_LIBPATHS "")
    foreach(_prefix ${CMAKE_PREFIX_PATH})
        get_filename_component(_lib_dir "${_prefix}/lib" ABSOLUTE)
        if(IS_DIRECTORY "${_lib_dir}")
            list(APPEND COPYQ_DEPLOY_LIBPATHS "${_lib_dir}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES COPYQ_DEPLOY_LIBPATHS)

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/mac/deploy.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/deploy_copyq.cmake"
        @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/deploy_copyq.cmake")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/mac/fixup_bundle.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/fixup_copyq_bundle.cmake"
        @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/fixup_copyq_bundle.cmake")

    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()

# Compile with tests?
OPTION(WITH_TESTS "Run test cases from command line" OFF)
if(WITH_TESTS)
    include(tests/tests.cmake)
endif()

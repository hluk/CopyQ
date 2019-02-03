project('copyq', ['cpp', 'c'], default_options : ['cpp_std=c++11'])

conf_data = configuration_data()
conf_data.set('version', 'v3.13.0')
configure_file(
  input : 'src/common/version.h.in',
  output : 'version.h',
  configuration : conf_data)

sources = [
  'src/app/app.cpp',
  'src/app/applicationexceptionhandler.cpp',
  'src/app/clipboardclient.cpp',
  'src/app/clipboardmonitor.cpp',
  'src/app/clipboardownermonitor.cpp',
  'src/app/clipboardserver.cpp',
  'src/common/action.cpp',
  'src/common/actionoutput.cpp',
  'src/common/actiontablemodel.cpp',
  'src/common/appconfig.cpp',
  'src/common/client_server.cpp',
  'src/common/clientsocket.cpp',
  'src/common/commandstore.cpp',
  'src/common/common.cpp',
  'src/common/config.cpp',
  'src/common/display.cpp',
  'src/common/globalshortcutcommands.cpp',
  'src/common/log.cpp',
  'src/common/messagehandlerforqt.cpp',
  'src/common/mimetypes.cpp',
  'src/common/option.cpp',
  'src/common/predefinedcommands.cpp',
  'src/common/sanitize_text_document.cpp',
  'src/common/server.cpp',
  'src/common/settings.cpp',
  'src/common/shortcuts.cpp',
  'src/common/tabs.cpp',
  'src/common/temporaryfile.cpp',
  'src/common/temporarysettings.cpp',
  'src/common/textdata.cpp',
  'src/gui/aboutdialog.cpp',
  'src/gui/actiondialog.cpp',
  'src/gui/actionhandler.cpp',
  'src/gui/actionhandlerdialog.cpp',
  'src/gui/addcommanddialog.cpp',
  'src/gui/clipboardbrowser.cpp',
  'src/gui/clipboardbrowserplaceholder.cpp',
  'src/gui/clipboarddialog.cpp',
  'src/gui/clipboardspy.cpp',
  'src/gui/commandaction.cpp',
  'src/gui/commandcompleter.cpp',
  'src/gui/commanddialog.cpp',
  'src/gui/commandedit.cpp',
  'src/gui/commandhelpbutton.cpp',
  'src/gui/commandsyntaxhighlighter.cpp',
  'src/gui/commandwidget.cpp',
  'src/gui/configtabappearance.cpp',
  'src/gui/configtabtabs.cpp',
  'src/gui/configurationmanager.cpp',
  'src/gui/execmenu.cpp',
  'src/gui/fancylineedit.cpp',
  'src/gui/filedialog.cpp',
  'src/gui/filtercompleter.cpp',
  'src/gui/filterlineedit.cpp',
  'src/gui/iconfactory.cpp',
  'src/gui/iconfont.cpp',
  'src/gui/iconselectbutton.cpp',
  'src/gui/iconselectdialog.cpp',
  'src/gui/iconwidget.cpp',
  'src/gui/importexportdialog.cpp',
  'src/gui/itemorderlist.cpp',
  'src/gui/logdialog.cpp',
  'src/gui/mainwindow.cpp',
  'src/gui/menuitems.cpp',
  'src/gui/notification.cpp',
  'src/gui/notificationdaemon.cpp',
  'src/gui/pluginwidget.cpp',
  'src/gui/screen.cpp',
  'src/gui/shortcutbutton.cpp',
  'src/gui/shortcutdialog.cpp',
  'src/gui/shortcutswidget.cpp',
  'src/gui/tabbar.cpp',
  'src/gui/tabdialog.cpp',
  'src/gui/tabicons.cpp',
  'src/gui/tabpropertieswidget.cpp',
  'src/gui/tabtree.cpp',
  'src/gui/tabwidget.cpp',
  'src/gui/theme.cpp',
  'src/gui/traymenu.cpp',
  'src/gui/windowgeometryguard.cpp',
  'src/item/clipboarditem.cpp',
  'src/item/clipboardmodel.cpp',
  'src/item/itemdelegate.cpp',
  'src/item/itemeditor.cpp',
  'src/item/itemeditorwidget.cpp',
  'src/item/itemfactory.cpp',
  'src/item/itemsaverwrapper.cpp',
  'src/item/itemstore.cpp',
  'src/item/itemwidget.cpp',
  'src/item/itemwidgetwrapper.cpp',
  'src/item/persistentdisplayitem.cpp',
  'src/item/serialize.cpp',
  'src/main.cpp',
  'src/scriptable/commandhelp.cpp',
  'src/scriptable/scriptable.cpp',
  'src/scriptable/scriptablebytearray.cpp',
  'src/scriptable/scriptabledir.cpp',
  'src/scriptable/scriptablefile.cpp',
  'src/scriptable/scriptableproxy.cpp',
  'src/scriptable/scriptabletemporaryfile.cpp',
  'src/tests/tests.cpp',
]

moc_headers = [
  'src/app/applicationexceptionhandler.h',
  'src/app/clipboardclient.h',
  'src/app/clipboardmonitor.h',
  'src/app/clipboardserver.h',
  'src/common/action.h',
  'src/common/clientsocket.h',
  'src/common/server.h',
  'src/gui/actiondialog.h',
  'src/gui/actionhandler.h',
  'src/gui/addcommanddialog.h',
  'src/gui/clipboardbrowser.h',
  'src/gui/clipboardbrowserplaceholder.h',
  'src/gui/clipboarddialog.h',
  'src/gui/clipboardspy.h',
  'src/gui/commandaction.h',
  'src/gui/commandcompleter.h',
  'src/gui/commanddialog.h',
  'src/gui/commandedit.h',
  'src/gui/commandhelpbutton.h',
  'src/gui/commandwidget.h',
  'src/gui/configtabtabs.h',
  'src/gui/configurationmanager.h',
  'src/gui/fancylineedit.h',
  'src/gui/filedialog.h',
  'src/gui/filtercompleter.h',
  'src/gui/filterlineedit.h',
  'src/gui/iconselectbutton.h',
  'src/gui/iconselectdialog.h',
  'src/gui/itemorderlist.h',
  'src/gui/mainwindow.h',
  'src/gui/notification.h',
  'src/gui/notificationdaemon.h',
  'src/gui/pluginwidget.h',
  'src/gui/shortcutbutton.h',
  'src/gui/shortcutswidget.h',
  'src/gui/tabbar.h',
  'src/gui/tabdialog.h',
  'src/gui/tabpropertieswidget.h',
  'src/gui/tabtree.h',
  'src/gui/tabwidget.h',
  'src/gui/traymenu.h',
  'src/item/itemdelegate.h',
  'src/item/itemeditor.h',
  'src/item/itemeditorwidget.h',
  'src/item/itemfactory.h',
  'src/scriptable/scriptable.h',
  'src/scriptable/scriptablebytearray.h',
  'src/scriptable/scriptabledir.h',
  'src/scriptable/scriptablefile.h',
  'src/scriptable/scriptableproxy.h',
  'src/scriptable/scriptabletemporaryfile.h',
  'src/tests/tests.h',
]

moc_sources = [
  'src/gui/notification.cpp',
]

ui_files = [
  'src/ui/mainwindow.ui',
  'src/ui/shortcutswidget.ui',
  'src/ui/configtabnotifications.ui',
  'src/ui/itemorderlist.ui',
  'src/ui/configtabtray.ui',
  'src/ui/configtabhistory.ui',
  'src/ui/addcommanddialog.ui',
  'src/ui/aboutdialog.ui',
  'src/ui/configurationmanager.ui',
  'src/ui/shortcutdialog.ui',
  'src/ui/commanddialog.ui',
  'src/ui/pluginwidget.ui',
  'src/ui/actiondialog.ui',
  'src/ui/logdialog.ui',
  'src/ui/tabdialog.ui',
  'src/ui/importexportdialog.ui',
  'src/ui/configtablayout.ui',
  'src/ui/tabpropertieswidget.ui',
  'src/ui/configtabappearance.ui',
  'src/ui/configtabgeneral.ui',
  'src/ui/commandedit.ui',
  'src/ui/actionhandlerdialog.ui',
  'src/ui/commandwidget.ui',
  'src/ui/clipboarddialog.ui',
]

translation_files = [
  'translations/copyq_ar.ts',
  'translations/copyq_cs.ts',
  'translations/copyq_da.ts',
  'translations/copyq_de.ts',
  'translations/copyq_es.ts',
  'translations/copyq_fr.ts',
  'translations/copyq_hu.ts',
  'translations/copyq_it.ts',
  'translations/copyq_ja.ts',
  'translations/copyq_lt.ts',
  'translations/copyq_nb.ts',
  'translations/copyq_nl.ts',
  'translations/copyq_pl.ts',
  'translations/copyq_pt_BR.ts',
  'translations/copyq_pt_PT.ts',
  'translations/copyq_ru.ts',
  'translations/copyq_sk.ts',
  'translations/copyq_sv.ts',
  'translations/copyq_tr.ts',
  'translations/copyq_uk.ts',
  'translations/copyq_zh_CN.ts',
  'translations/copyq_zh_TW.ts',
]

qresources = [
  'src/copyq.qrc',
]

qt_modules = [
  'Gui',
  'Network',
  'Qml',
  'Svg',
  'Test',
  'Widgets',
  'Xml',
  'X11Extras',
]

qt5 = import('qt5')

platform_moc_headers = [
  'src/platform/platformclipboard.h',
]

use_qxt = false
host_system = host_machine.system()
dependencies = []
link_with = []
# https://mesonbuild.com/Reference-tables.html#operating-system-names
if host_system == 'linux'
  platform_sources = [
    'qxt/qxtglobalshortcut_x11.cpp',
    'src/platform/dummy/dummyclipboard.cpp',
    'src/platform/platformcommon.cpp',
    'src/platform/unix/unixsignalhandler.cpp',
    'src/platform/x11/systemclipboard/systemclipboard.cpp',
    'src/platform/x11/systemclipboard/waylandclipboard.cpp',
    'src/platform/x11/x11platform.cpp',
    'src/platform/x11/x11platformclipboard.cpp',
    'src/platform/x11/x11platformwindow.cpp',
  ]
  moc_headers += [
    'src/platform/x11/systemclipboard/systemclipboard.h',
  ]
  moc_sources += [
    'src/platform/x11/systemclipboard/waylandclipboard.cpp',
  ]
  use_qxt = true
  qt_modules += [
    'WaylandClient',
    'X11Extras',
  ]

  pkg = import('pkgconfig')
  x11 = dependency('x11')
  wayland_client = dependency('wayland-client', version : '>=1.15')
  dependencies += [wayland_client, x11]

  # FIXME: Get the real Qt installation path.
  qt_binaries_dir = '/usr/lib64/qt5/bin'
  qt_wayland_scanner = find_program(
    'qtwaylandscanner',
    qt_binaries_dir / 'qtwaylandscanner',
    required : true)
  wayland_scanner = find_program('wayland-scanner', required : true)

  wlr_xml = 'src/platform/x11/systemclipboard/wlr-data-control-unstable-v1.xml'
  wlr_data_list = [
    {
      'arg': 'client-header',
      'output': 'wayland-wlr-data-control-unstable-v1-client-protocol.h',
    },
    {
      'arg': 'code',
      'output': 'wayland-wlr-data-control-unstable-v1-protocol.c',
    },
  ]
  foreach item : wlr_data_list
    generated = custom_target(
      item.get('output'),
      command : [wayland_scanner, item.get('arg'), '@INPUT@', '@OUTPUT@'],
      input : [wlr_xml],
      output : [item.get('output')],
    )
    sources += [generated]
  endforeach

  qt_wlr_data_list = [
    {
      'arg': 'client-header',
      'output': 'qwayland-wlr-data-control-unstable-v1.h',
    },
    {
      'arg': 'client-code',
      'output': 'qwayland-wlr-data-control-unstable-v1.cpp',
    },
  ]
  foreach item : qt_wlr_data_list
    generated = custom_target(
      item.get('output'),
      command : [qt_wayland_scanner, item.get('arg'), '@INPUT@', ''],
      capture : true,
      input : [wlr_xml],
      output : [item.get('output')],
    )
    sources += [generated]
  endforeach
elif host_system == 'windows'
  platform_sources = [
    'qxt/qxtglobalshortcut_win.cpp',
    'src/platform/dummy/dummyclipboard.cpp',
    'src/platform/platformcommon.cpp',
    'src/platform/win/winplatform.cpp',
    'src/platform/win/winplatformclipboard.cpp',
    'src/platform/win/winplatformwindow.cpp',
  ]
  use_qxt = true
  qt_modules += ['MacExtras']
elif host_system == 'darwin'
  platform_sources = [
    'qxt/qxtglobalshortcut_mac.cpp',
    'src/platform/dummy/dummyclipboard.cpp',
    'src/platform/unix/unixsignalhandler.cpp',
    'src/platform/mac/macplatformwindow.mm',
    'src/platform/mac/foregroundbackgroundfilter.mm',
    'src/platform/mac/macplatform.mm',
    'src/platform/mac/urlpasteboardmime.mm',
    'src/platform/mac/macclipboard.mm',
    'src/platform/mac/mactimer.mm',
    'src/platform/mac/copyqpasteboardmime.mm',
    'src/platform/mac/macactivity.mm',
  ]
  use_qxt = true
else
  platform_sources = [
    'src/platform/dummy/dummyclipboard.cpp',
    'src/platform/dummy/dummyplatform.cpp',
    'src/platform/platformcommon.cpp',
    'src/platform/unix/unixsignalhandler.cpp',
  ]
endif

if use_qxt
  sources += ['qxt/qxtglobalshortcut.cpp']
  moc_headers += ['qxt/qxtglobalshortcut.h']
endif

qt5_dependencies = dependency('qt5', modules : qt_modules, private_headers : true)

translations = qt5.compile_translations(
  ts_files : translation_files,
  build_by_default : true,
)

qt5_processed = qt5.preprocess(
  moc_headers : [moc_headers, platform_moc_headers],
  moc_sources : moc_sources,
  ui_files : ui_files,
  qresources : qresources,
)

dependencies += [
  qt5_dependencies
]

include_dirs = include_directories('src')

executable(
  'copyq',
  sources : [sources, platform_sources, qt5_processed],
  dependencies : dependencies,
  include_directories : include_dirs,
  link_with : link_with,
)

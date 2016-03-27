# Writeback modules are tracked here as arrays with the following elements:
#
#  [ name, source-files, dependencies ]
#
# This might be nicer if Meson supported dictionary types.

modules = []

if taglib.found()
  modules += [['writeback-taglib', 'tracker-writeback-taglib.c', [tracker_common_dep, tracker_miner_dep, tracker_sparql_dep, taglib]]]
endif

if exempi.found()
  modules += [['writeback-xmp', 'tracker-writeback-xmp.c', [tracker_common_dep, tracker_miner_dep, tracker_sparql_dep, exempi]]]
endif

foreach module : modules
  name = module[0]
  sources = module[1]
  dependencies = module[2]

  shared_module(name, sources,
    c_args: [ '-DTRACKER_COMPILATION' ],
    dependencies: dependencies,
    include_directories: configinc,
    install: true,
    install_dir: tracker_writeback_modules_dir)
endforeach

tracker_writeback_sources = [
  'tracker-config.c',
  'tracker-config.h',
  'tracker-writeback-file.c',
  'tracker-writeback-file.h',
  'tracker-writeback-module.c',
  'tracker-writeback-module.h',
  'tracker-writeback.c',
  'tracker-writeback.h',
  'tracker-main.c'
]

executable('tracker-writeback',
  tracker_writeback_sources,
  c_args: [
    '-DTRACKER_COMPILATION',
    '-DWRITEBACK_MODULES_DIR="@0@"'.format(tracker_writeback_modules_dir),
  ],
  dependencies: [gmodule, tracker_common_dep, tracker_miner_dep, tracker_sparql_dep],
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('libexecdir')))

install_data(
  'tracker-writeback.xml',
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'tracker'))

dbus_service_file = configure_file(
  input: 'org.freedesktop.Tracker1.Writeback.service.in',
  output: 'org.freedesktop.Tracker1.Writeback.service',
  configuration: conf,
  install: true,
  install_dir: dbus_services_dir)

custom_target('tracker-writeback-settings-schema',
  input: 'org.freedesktop.Tracker.Writeback.gschema.xml.in',
  output: 'org.freedesktop.Tracker.Writeback.gschema.xml',
  command: ['intltool-merge', '--quiet', '--xml-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
  install: true,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

if install_systemd_user_services
  configure_file(
    input: 'tracker-writeback.service.in',
    output: 'tracker-writeback.service',
    configuration: conf,
    install: true,
    install_dir: systemd_user_services_dir)
endif

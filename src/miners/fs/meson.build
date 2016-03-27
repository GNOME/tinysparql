sources = [
    'tracker-config.c',
    'tracker-extract-watchdog.c',
    'tracker-main.c',
    'tracker-miner-files.c',
    'tracker-miner-files-index.c',
    'tracker-miner-files-peer-listener.c',
    'tracker-storage.c',
    'tracker-writeback-listener.c',
    'tracker-writeback-dispatcher.c',
    'tracker-writeback.c',
]

if battery_detection_library_name == 'upower'
  sources += 'tracker-power-upower.c'
elif battery_detection_library_name == 'hal'
  sources += 'tracker-power-hal.c'
endif

executable('tracker-miner-fs', sources,
    dependencies: [tracker_common_dep, tracker_miner_dep, tracker_extract_dep],
    c_args: [
        '-DTRACKER_COMPILATION',
    ],
    install: true,
    install_dir: get_option('libexecdir'),
    install_rpath: tracker_internal_libs_dir,
)

install_data(
    sources: 'tracker-miner-files-index.xml',
    install_dir: join_paths(get_option('datadir'), 'tracker'))

dbus_service_file_untranslated = configure_file(
    input: 'org.freedesktop.Tracker1.Miner.Files.service.in.in',
    output: 'org.freedesktop.Tracker1.Miner.Files.service.in',
    configuration: conf)

dbus_service_file = custom_target('tracker-miner-files-dbus-service-file',
    input: dbus_service_file_untranslated,
    output: 'org.freedesktop.Tracker1.Miner.Files.service',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: dbus_services_dir)

desktop_file_untranslated = configure_file(
    input: 'tracker-miner-fs.desktop.in.in',
    output: 'tracker-miner-fs.desktop.in',
    configuration: conf)

desktop_file = custom_target('tracker-miner-fs-desktop-file',
    input: desktop_file_untranslated,
    output: 'tracker-miner-fs.desktop',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('sysconfdir'), 'xdg/autostart'))

custom_target('tracker-miner-fs-settings-schema',
    input: 'org.freedesktop.Tracker.Miner.Files.gschema.xml.in',
    output: 'org.freedesktop.Tracker.Miner.Files.gschema.xml',
    command: ['intltool-merge', '--quiet', '--xml-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

if install_systemd_user_services
  configure_file(
      input: 'tracker-miner-fs.service.in',
      output: 'tracker-miner-fs.service',
      configuration: conf,
      install: true,
      install_dir: systemd_user_services_dir)
endif

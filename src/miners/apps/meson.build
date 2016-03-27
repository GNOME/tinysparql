sources = [
    'tracker-main.c',
    'tracker-miner-applications.c',
]

executable('tracker-miner-apps', sources,
    dependencies: [tracker_common_dep, tracker_miner_dep, tracker_sparql_dep],
    c_args: [ '-DTRACKER_COMPILATION', ],
    install: true,
    install_dir: get_option('libexecdir'),
    install_rpath: tracker_internal_libs_dir,
)

dbus_service_file_untranslated = configure_file(
    input: 'org.freedesktop.Tracker1.Miner.Applications.service.in.in',
    output: 'org.freedesktop.Tracker1.Miner.Applications.service.in',
    configuration: conf)

dbus_service_file = custom_target('tracker-miner-apps-dbus-service-file',
    input: dbus_service_file_untranslated,
    output: 'org.freedesktop.Tracker1.Miner.Applications.service',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: dbus_services_dir)

desktop_file_untranslated = configure_file(
    input: 'tracker-miner-apps.desktop.in.in',
    output: 'tracker-miner-apps.desktop.in',
    configuration: conf)

desktop_file = custom_target('tracker-miner-apps-desktop-file',
    input: desktop_file_untranslated,
    output: 'tracker-miner-apps.desktop',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('sysconfdir'), 'xdg/autostart'))

if install_systemd_user_services
  configure_file(
      input: 'tracker-miner-apps.service.in',
      output: 'tracker-miner-apps.service',
      configuration: conf,
      install: true,
      install_dir: systemd_user_services_dir)
endif

sources = [
    'tracker-main.c',
    'tracker-miner-rss.c',
]

executable('tracker-miner-rss', sources,
    dependencies: [libgrss, tracker_common_dep, tracker_miner_dep, tracker_extract_dep],
    c_args: [ '-DTRACKER_COMPILATION', ],
    install_rpath: tracker_internal_libs_dir,
)

dbus_service_file_untranslated = configure_file(
    input: 'org.freedesktop.Tracker1.Miner.RSS.service.in.in',
    output: 'org.freedesktop.Tracker1.Miner.RSS.service.in',
    configuration: conf)

dbus_service_file = custom_target('tracker-miner-rss-dbus-service-file',
    input: dbus_service_file_untranslated,
    output: 'org.freedesktop.Tracker1.Miner.RSS.service',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: dbus_services_dir)

desktop_file_untranslated = configure_file(
    input: 'tracker-miner-rss.desktop.in.in',
    output: 'tracker-miner-rss.desktop.in',
    configuration: conf)

desktop_file = custom_target('tracker-miner-rss-desktop-file',
    input: desktop_file_untranslated,
    output: 'tracker-miner-rss.desktop',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('sysconfdir'), 'xdg/autostart'))

if install_systemd_user_services
  configure_file(
      input: 'tracker-miner-rss.service.in',
      output: 'tracker-miner-rss.service',
      configuration: conf,
      install: true,
      install_dir: systemd_user_services_dir)
endif

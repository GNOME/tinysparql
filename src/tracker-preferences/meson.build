executable(
    'tracker-preferences',
    'config.vapi',
    'tracker-preferences.vala',
    '../miners/fs/tracker-config.c',
    dependencies: [tracker_common_dep, tracker_sparql_dep, gmodule, gtk3],
    c_args: [
        '-D_XOPEN_SOURCE',
        '-D_XOPEN_SOURCE_EXTENDED',
        '-DTRACKER_COMPILATION',
        '-include', 'config.h'
    ],
    vala_args: [
        '--pkg', 'posix'
    ],
    install: true,
    install_rpath: tracker_internal_libs_dir
)

desktop_file_untranslated = configure_file(
    input: 'tracker-preferences.desktop.in.in',
    output: 'tracker-preferences.desktop.in',
    configuration: conf)

desktop_file = custom_target('tracker-preferences-desktop-file',
    input: desktop_file_untranslated,
    output: 'tracker-preferences.desktop',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'applications'))

install_data(
    'tracker-preferences.appdata.xml',
    install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'appdata'))

install_data(
    'tracker-preferences.ui',
    install_dir: join_paths(get_option('prefix'), get_option('datadir')))

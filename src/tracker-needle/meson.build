executable(
    'tracker-needle',
    'config.vapi',
    'tracker-needle.vala',
    'tracker-history.vala',
    'tracker-needle.vala',
    'tracker-query.vala',
    'tracker-result-store.vala',
    'tracker-stats.vala',
    'tracker-tags-view.vala',
    'tracker-utils.vala',
    'tracker-view.vala',
    dependencies: [tracker_common_dep, tracker_sparql_dep, gtk3],
    c_args: [
        '-D_XOPEN_SOURCE',
        '-D_XOPEN_SOURCE_EXTENDED',
        '-include', 'config.h'
    ],
    install: true,
    install_rpath: tracker_internal_libs_dir
)

custom_target('tracker-needle-settings-schema',
    input: 'org.freedesktop.Tracker.Needle.gschema.xml.in',
    output: 'org.freedesktop.Tracker.Needle.gschema.xml',
    command: ['intltool-merge', '--quiet', '--xml-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas'))

install_data('tracker-needle.ui',
    install_dir: join_paths(get_option('datadir'), 'tracker'))

desktop_file_untranslated = configure_file(
    input: 'tracker-needle.desktop.in.in',
    output: 'tracker-needle.desktop.in',
    configuration: conf)

desktop_file = custom_target('tracker-needle-desktop-file',
    input: desktop_file_untranslated,
    output: 'tracker-needle.desktop',
    command: ['intltool-merge', '--quiet', '--desktop-style', '--utf8', join_paths(meson.source_root(), 'po'), '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(get_option('datadir'), 'applications'))

install_data('tracker-needle.appdata.xml',
    install_dir: join_paths(get_option('datadir'), 'tracker-needle.appdata.xml'))

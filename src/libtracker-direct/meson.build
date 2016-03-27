libtracker_direct = static_library('tracker-direct',
    'tracker-direct.vala',
    'tracker-namespace.vala',
    '../libtracker-common/libtracker-common.vapi',
    '../libtracker-data/libtracker-data.vapi',
    c_args: [
        '-DTRACKER_COMPILATION',
    ],
    vala_args: [
        '--debug',
        '--pkg', 'posix',
        # FIXME: Meson has code to add --target-glib automatically, but it
        # doesn't seem to work here.
        '--target-glib', glib_required,
    ],
    # This doesn't depend on tracker_common_dep because of
    # https://github.com/mesonbuild/meson/issues/671
    dependencies: [ glib, gio, tracker_data_dep ],
    include_directories: [commoninc, configinc, srcinc],
)

tracker_sparql_direct_dep = declare_dependency(
    link_with: libtracker_direct,
    include_directories: include_directories('.')
)

libtracker_bus = static_library('tracker-bus',
    'tracker-bus.vala',
    'tracker-namespace.vala',
    'tracker-array-cursor.vala',
    'tracker-bus-fd-cursor.vala',
    '../libtracker-common/libtracker-common.vapi',
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
    dependencies: [glib, gio, gio_unix, tracker_sparql_intermediate_dep],
    include_directories: [commoninc, configinc, srcinc],
    link_with: libtracker_common,
    sources: tracker_common_enum_header,
)

tracker_sparql_bus_dep = declare_dependency(
    link_with: libtracker_bus,
    include_directories: include_directories('.')
)

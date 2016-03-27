sources = ['tracker-miner-manager.c']

headers = ['tracker-control.h', 'tracker-miner-manager.h']

libtracker_control = library('tracker-control-' + tracker_api_version,
    sources,
    c_args: '-DTRACKER_COMPILATION',
    install: true,
    install_rpath: tracker_internal_libs_dir,
    # This doesn't depend on tracker_common_dep because of
    # https://github.com/mesonbuild/meson/issues/671
    dependencies: [gio, tracker_sparql_dep, tracker_miner_dep],
    include_directories: [commoninc, configinc, srcinc],
    link_with: libtracker_common,
    sources: tracker_common_enum_header,
)

tracker_control_dep = declare_dependency(
    link_with: libtracker_control,
    dependencies: tracker_miner_dep
)

tracker_control_gir = gnome.generate_gir(libtracker_control,
    sources: sources + headers,
    nsversion: tracker_api_version,
    namespace: 'TrackerControl',
    identifier_prefix: 'Tracker',
    symbol_prefix: 'tracker',
    includes : ['GLib-2.0', 'GObject-2.0', 'Gio-2.0' ],
    install: true,
    extra_args: '--c-include=libtracker-control/tracker-control.h')

configure_file(
    input: 'tracker-control.pc.in',
    output: 'tracker-control-1.0.pc',
    configuration: conf,
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'pkgconfig'))

install_headers (headers,
    subdir: 'tracker-1.0/libtracker-control')

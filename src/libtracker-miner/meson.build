shared_libtracker_miner_monitor_sources = files('tracker-monitor.c')
shared_libtracker_miner_file_system_sources = files('tracker-file-system.c')
shared_libtracker_miner_crawler_sources = files('tracker-crawler.c')

miner_enums = gnome.mkenums('tracker-miner-enum-types',
    sources: 'tracker-miner-enums.h',
    c_template: 'tracker-miner-enum-types.c.template',
    h_template: 'tracker-miner-enum-types.h.template',
)

private_sources = [
    'tracker-crawler.c',
    'tracker-file-data-provider.c',
    'tracker-file-enumerator.c',
    'tracker-file-notifier.c',
    'tracker-file-system.c',
    'tracker-priority-queue.c',
    'tracker-task-pool.c',
    'tracker-sparql-buffer.c',
    'tracker-thumbnailer.c',
    'tracker-utils.c']

if libmediaart.found()
    private_sources.append(['tracker-media-art.c'])
endif

miner_headers = [
    'tracker-enumerator.h',
    'tracker-miner-online.h',
    'tracker-data-provider.h',
    'tracker-indexing-tree.h',
    'tracker-decorator-fs.h',
    'tracker-miner-fs.h',
    'tracker-miner-object.h',
    'tracker-decorator.h',
    'tracker-miner-enums.h',
    'tracker-miner.h',
]

miner_sources = (
    shared_libtracker_miner_monitor_sources +
    shared_libtracker_miner_file_system_sources +
    shared_libtracker_miner_crawler_sources +
    ['tracker-data-provider.c',
    'tracker-decorator.c',
    'tracker-decorator-fs.c',
    'tracker-enumerator.c',
    'tracker-indexing-tree.c',
    'tracker-miner-object.c',
    'tracker-miner-online.c',
    'tracker-miner-fs.c'])

libtracker_miner_private = static_library(
    'tracker-miner-private',
    miner_enums[0], miner_enums[1], private_sources,
    dependencies: [tracker_common_dep, tracker_sparql_dep],
    c_args: [
        '-DTRACKER_COMPILATION',
    ],
)

tracker_miner_dependencies = []
if network_manager.found()
    tracker_miner_dependencies += network_manager
endif

libtracker_miner = library(
    'tracker-miner-' + tracker_api_version,
    miner_enums[0], miner_enums[1], miner_sources,
    c_args: [ '-DTRACKER_COMPILATION', ],
    install: true,
    install_rpath: tracker_internal_libs_dir,
    # This doesn't depend on tracker_common_dep because of
    # https://github.com/mesonbuild/meson/issues/671
    include_directories: [commoninc, configinc, srcinc],
    dependencies: [tracker_sparql_dep] + tracker_miner_dependencies,
    link_with: [libtracker_miner_private],
)

tracker_miner_dep = declare_dependency(
    sources: miner_enums[1],
    link_with: libtracker_miner,
    include_directories: include_directories('.')
)

tracker_miner_gir = gnome.generate_gir(libtracker_miner,
    sources: miner_sources + miner_headers,
    nsversion: tracker_api_version,
    namespace: 'TrackerMiner',
    identifier_prefix: 'Tracker',
    symbol_prefix: 'tracker',
    # FIXME: also depends on Tracker-1.0.gir (output of libtracker-sparql)
    # but we can't currently access that from the Vala target
    includes : ['GLib-2.0', 'GObject-2.0', 'Gio-2.0' ],
    install: true,
    extra_args: [
        '--c-include=libtracker-miner/tracker-miner.h',
        '-D', 'TRACKER_COMPILATION',
    ])

configure_file(
    input: 'tracker-miner.pc.in',
    output: 'tracker-miner-1.0.pc',
    configuration: conf,
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'pkgconfig'))

install_headers(miner_headers, subdir: 'tracker-1.0/libtracker-miner')

# Work around https://github.com/mesonbuild/meson/issues/705
meson.add_install_script('../install-generated-header.sh',
    join_paths(meson.current_build_dir(), 'tracker-miner-enum-types.h'),
    join_paths(get_option('prefix'), get_option('includedir'), 'tracker-1.0', 'libtracker-miner'))

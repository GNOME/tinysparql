# Vala parts of libtracker-data are split out separately, because we can't link
# the Vala code to libicu. In the Makefile.am this library is called
# libtracker-sparql-query.
libtracker_data_vala = static_library('tracker-sparql-query',
    'tracker-vala-namespace.vala',
    'tracker-sparql-query.vala',
    'tracker-sparql-expression.vala',
    'tracker-sparql-pattern.vala',
    'tracker-sparql-scanner.vala',
    'tracker-turtle-reader.vala',
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
    dependencies: [glib, gio, tracker_sparql_intermediate_dep],
    include_directories: [commoninc, configinc, srcinc],
    sources: tracker_common_enum_header,
)

tracker_data_dependencies = [
    tracker_sparql_intermediate_dep, gvdb_dep, sqlite, uuid,
]

if get_option('fts')
    tracker_data_dependencies += tracker_fts_dep
endif

 #[gio_unix, glib, libmath, sqlite, uuid]
libtracker_data = library('tracker-data',
    'tracker-class.c',
    'tracker-collation.c',
    'tracker-crc32.c',
    'tracker-data-backup.c',
    'tracker-data-manager.c',
    'tracker-data-query.c',
    'tracker-data-update.c',
    'tracker-db-config.c',
    'tracker-db-interface.c',
    'tracker-db-interface-sqlite.c',
    'tracker-db-manager.c',
    'tracker-db-journal.c',
    'tracker-db-backup.c',
    'tracker-namespace.c',
    'tracker-ontology.c',
    'tracker-ontologies.c',
    'tracker-property.c',
    c_args: [
        '-DTRACKER_COMPILATION',
    ],
    install: true,
    install_dir: tracker_internal_libs_dir,
    # This doesn't depend on tracker_common_dep because of
    # https://github.com/mesonbuild/meson/issues/671
    link_with: [libtracker_data_vala],
    dependencies: tracker_data_dependencies + [unicode_library],
    include_directories: [commoninc, configinc, srcinc],
)

tracker_data_dep = declare_dependency(
    link_with: [libtracker_data, libtracker_data_vala],
    dependencies: tracker_data_dependencies,
    include_directories: include_directories('.')
)

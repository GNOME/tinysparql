version_header = configure_file(
    input: 'tracker-version-generated.h.meson.in',
    output: 'tracker-version-generated.h',
    configuration: conf)

enum_types = gnome.mkenums('tracker-sparql-enum-types',
    sources: ['tracker-notifier.h', 'tracker-connection.h'],
    c_template: 'tracker-sparql-enum-types.c.template',
    h_template: 'tracker-sparql-enum-types.h.template',
    install_dir: join_paths(get_option('prefix'), get_option('includedir'), 'tracker-@0@'.format(tracker_api_version), 'libtracker-sparql'),
    install_header: true,
)

tracker_sparql_vapi = files('tracker-sparql.vapi')
tracker_sparql_vapi_dep = meson.get_compiler('vala').find_library('tracker-sparql', dirs: meson.current_source_dir())

libtracker_sparql_c_sources = files(
    'tracker-batch.c',
    'tracker-connection.c',
    'tracker-cursor.c',
    'tracker-endpoint.c',
    'tracker-endpoint-dbus.c',
    'tracker-error.c',
    'tracker-namespace-manager.c',
    'tracker-notifier.c',
    'tracker-resource.c',
    'tracker-statement.c',
    'tracker-serializer.c',
    'tracker-serializer-json.c',
    'tracker-serializer-xml.c',
    'tracker-uri.c',
    'tracker-utils.c',
    'tracker-version.c',
)

libtracker_sparql_c_public_headers = files(
    'tracker-batch.h',
    'tracker-connection.h',
    'tracker-cursor.h',
    'tracker-endpoint.h',
    'tracker-endpoint-dbus.h',
    'tracker-endpoint-http.h',
    'tracker-error.h',
    'tracker-namespace-manager.h',
    'tracker-notifier.h',
    'tracker-ontologies.h',
    'tracker-resource.h',
    'tracker-statement.h',
    'tracker-utils.h',
    'tracker-version.h',
)

libtracker_sparql_intermediate = static_library('tracker-sparql-intermediate',
    enum_types,
    libtracker_sparql_c_sources,
    dependencies: [tracker_common_dep, json_glib, libxml2],
    gnu_symbol_visibility: 'hidden',
)

sparqlinc = [include_directories('.'), meson.current_build_dir()]

tracker_sparql_intermediate_dep = declare_dependency(
    link_with: [libtracker_sparql_intermediate],
    include_directories: [srcinc, include_directories('.')],
    dependencies: [ tracker_sparql_vapi_dep, tracker_data_dep ],
    sources: enum_types[1],
)

install_headers(
    libtracker_sparql_c_public_headers,
    'tracker-ontologies.h',
    'tracker-sparql.h',
    version_header,
    subdir: 'tracker-@0@/libtracker-sparql'.format(tracker_api_version))

install_data(
    tracker_sparql_vapi,
    rename: ['tracker-sparql-' + tracker_api_version + '.vapi'],
    install_dir: vapi_dir,
)

install_data(
    'tracker-sparql.deps',
    rename: ['tracker-sparql-' + tracker_api_version + '.deps'],
    install_dir: vapi_dir,
)

subdir('bus')
subdir('direct')

tracker_remote_dependencies = [json_glib, libxml2]

libtracker_sparql_remote_c_sources = files (
    'tracker-endpoint-http.c',
    'remote/tracker-remote-statement.c',
)

remote_sources = [
    libtracker_sparql_remote_c_sources,
    'remote/tracker-remote.vapi',
    'remote/tracker-json-cursor.vala',
    'remote/tracker-xml-cursor.vala',
    'remote/tracker-remote.vala',
]

if libsoup2.found()
    libtracker_remote_soup2 = shared_module('tracker-remote-soup2', remote_sources,
        dependencies: tracker_remote_dependencies + [tracker_common_dep, tracker_sparql_intermediate_dep, libsoup2],
        c_args: tracker_c_args + [
            '-include', 'config.h',
            '-include', 'libtracker-sparql/tracker-private.h',
        ],
        vala_args: [
            '--debug',
            '--pkg', 'posix',
            # FIXME: Meson has code to add --target-glib automatically, but it
            # doesn't seem to work here.
            '--target-glib', glib_required,
            '--define=SOUP2',
        ],
        install: true,
        install_dir: tracker_internal_libs_dir,
    )
endif

if libsoup3.found()
    libtracker_remote_soup3 = shared_module('tracker-remote-soup3', remote_sources,
        dependencies: tracker_remote_dependencies + [tracker_common_dep, tracker_sparql_intermediate_dep, libsoup3],
        c_args: tracker_c_args + [
            '-include', 'config.h',
            '-include', 'libtracker-sparql/tracker-private.h',
        ],
        vala_args: [
            '--debug',
            '--pkg', 'posix',
            # FIXME: Meson has code to add --target-glib automatically, but it
            # doesn't seem to work here.
            '--target-glib', glib_required,
        ],
        install: true,
        install_dir: tracker_internal_libs_dir,
    )
endif

libtracker_sparql = library('tracker-sparql-' + tracker_api_version,
    '../libtracker-common/libtracker-common.vapi',
    '../libtracker-data/libtracker-data.vapi',
    'direct/tracker-direct.vapi',
    'tracker-backend.vala',
    'tracker-remote-module.c',
    tracker_gresources,

    gnu_symbol_visibility: 'hidden',

    soversion: soversion,
    version: libversion,

    install: true,
    install_rpath: tracker_internal_libs_dir,

    c_args: [
	'-include', 'libtracker-sparql/tracker-private.h',
        '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
        '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
        '-DBUILDROOT="@0@"'.format(meson.build_root()),
    ],

    link_whole: [libtracker_sparql_intermediate],

    dependencies: [tracker_common_dep, tracker_sparql_bus_dep, tracker_sparql_direct_dep, tracker_sparql_vapi_dep, gmodule, libdl],
)

tracker_sparql_dep = declare_dependency(
    link_with: libtracker_sparql,
    include_directories: srcinc,
    dependencies: [glib, gio],
)

pkg.generate(libtracker_sparql,
    description: 'Tracker : A SPARQL triple store library',
    requires: [glib, gio, gobject],
    subdirs: [
        'tracker-' + tracker_api_version,
        'tracker-' + tracker_api_version / 'libtracker-sparql',
    ],
    variables: [
        'exec_prefix=${prefix}',
        'libexecdir=${prefix}' / get_option('libexecdir'),
        'datadir=${prefix}' / get_option('datadir'),
        'tracker_datadir=${datadir}' / tracker_versioned_name,
        'ontologies_dir=' + tracker_ontologies_dir,
    ],
)

if get_option('introspection').enabled()
    tracker_sparql_gir = gnome.generate_gir(libtracker_sparql,
        sources: libtracker_sparql_c_sources + libtracker_sparql_c_public_headers,
        nsversion: tracker_api_version,
        namespace: 'Tracker',
        identifier_prefix: 'Tracker',
        symbol_prefix: 'tracker',
        includes : ['GLib-2.0', 'GObject-2.0', 'Gio-2.0' ],
        link_with: libtracker_sparql,
        install: true,
        extra_args: [
            '--c-include',
            'libtracker-sparql/tracker-sparql.h',
            '-DTRACKER_COMPILATION',
        ],
        export_packages: 'tracker-sparql-3.0')
endif
tracker_sparql_uninstalled_dir = meson.current_build_dir()

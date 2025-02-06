libtracker_sparql_modules = []

subdir('core')
subdir('bus')
subdir('direct')
subdir('remote')

version_header = configure_file(
    input: 'tracker-version-generated.h.meson.in',
    output: 'tracker-version-generated.h',
    configuration: conf)

enum_types = gnome.mkenums_simple('tracker-sparql-enum-types',
    sources: ['tracker-notifier.h', 'tracker-connection.h', 'tracker-enums.h', 'tracker-error.h'],
    install_dir: join_paths(get_option('prefix'), get_option('includedir'), 'tinysparql-@0@'.format(tracker_api_version)),
    install_header: true,
    decorator: 'TRACKER_AVAILABLE_IN_ALL',
    body_prefix: '#include "config.h"',
    header_prefix: '#include <tracker-version.h>\n',
)

libtracker_sparql_sources = files(
    'tracker-batch.c',
    'tracker-connection.c',
    'tracker-cursor.c',
    'tracker-deserializer.c',
    'tracker-deserializer-rdf.c',
    'tracker-deserializer-turtle.c',
    'tracker-deserializer-json.c',
    'tracker-deserializer-json-ld.c',
    'tracker-deserializer-resource.c',
    'tracker-deserializer-xml.c',
    'tracker-endpoint.c',
    'tracker-endpoint-dbus.c',
    'tracker-endpoint-http.c',
    'tracker-error.c',
    'tracker-namespace-manager.c',
    'tracker-notifier.c',
    'tracker-resource.c',
    'tracker-statement.c',
    'tracker-serializer.c',
    'tracker-serializer-json.c',
    'tracker-serializer-json-ld.c',
    'tracker-serializer-trig.c',
    'tracker-serializer-turtle.c',
    'tracker-serializer-xml.c',
    'tracker-uri.c',
    'tracker-utils.c',
    'tracker-version.c',
)

libtracker_sparql_public_headers = files(
    'tracker-batch.h',
    'tracker-connection.h',
    'tracker-cursor.h',
    'tracker-endpoint.h',
    'tracker-endpoint-dbus.h',
    'tracker-endpoint-http.h',
    'tracker-enums.h',
    'tracker-error.h',
    'tracker-namespace-manager.h',
    'tracker-notifier.h',
    'tracker-ontologies.h',
    'tracker-resource.h',
    'tracker-statement.h',
    'tracker-utils.h',
    'tracker-version.h',
)

sparqlinc = [meson.current_source_dir(), meson.current_build_dir()]

install_headers(
    libtracker_sparql_public_headers,
    'tracker-ontologies.h',
    'tinysparql.h',
    version_header,
    subdir: 'tinysparql-@0@'.format(tracker_api_version))

install_headers(
    'tracker-sparql.h',
    subdir: 'tinysparql-@0@/libtracker-sparql'.format(tracker_api_version))

libtracker_sparql_dependencies = [
    tracker_common_dep,
    tsparql_http_dep,
    gmodule,
    libdl,
    json_glib,
    libxml2,
    tracker_data_dependencies
]

libtracker_sparql_private = static_library('tracker-sparql-private',
    enum_types,
    libtracker_sparql_sources,
    core_files,
    bus_files,
    direct_files,
    remote_files,
    tracker_gresources,
    tracker_nepomuk_gresources,

    c_args: [
	'-include', 'tracker-private.h',
        '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
        '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
        # Global build root is required to detect noinst situations (tests/introspection),
        # event when built as a subproject
        '-DBUILDROOT="@0@"'.format(meson.global_build_root()),
    ] + tracker_c_args,

    dependencies: [libtracker_sparql_dependencies],
    gnu_symbol_visibility: 'hidden',
    include_directories: [core_incs, httpinc, ontology_incs],
)

tracker_sparql_private_dep = declare_dependency(
    link_with: [libtracker_sparql_private],
    include_directories: [srcinc, libinc],
    sources: enum_types[1],
    dependencies: [glib, gio],
)

libtracker_sparql = library('tinysparql-' + tracker_api_version,
    gnu_symbol_visibility: 'hidden',

    link_whole: [libtracker_sparql_private],

    soversion: soversion,
    version: libversion,

    install: true,
    install_rpath: tracker_internal_libs_dir,

    dependencies: [libtracker_sparql_dependencies],
)

tracker_sparql_dep = declare_dependency(
    link_with: libtracker_sparql,
    include_directories: [srcinc, libinc],
    dependencies: [glib, gio],
    sources: enum_types[1],
)

pkg.generate(libtracker_sparql,
    name: 'tinysparql-3.0',
    description: 'TinySPARQL : A SPARQL triple store library',
    libraries: [glib, gio, gobject],
    subdirs: [
        'tinysparql-' + tracker_api_version,
    ],
)

pkg.generate(libtracker_sparql,
    name: 'tracker-sparql-3.0',
    description: 'TinySPARQL : A SPARQL triple store library',
    libraries: [glib, gio, gobject, libtracker_sparql, libmath, libdl],
    subdirs: [
        'tinysparql-' + tracker_api_version,
        'tinysparql-' + tracker_api_version / 'libtracker-sparql',
    ],
    variables: [
        'exec_prefix=${prefix}',
        'tracker_datadir=${datadir}/' + tracker_versioned_name,
    ],
)

if get_option('introspection').enabled()
    # These are various hacks to ensure the GIR file depends on the
    # remote connection modules as they are needed at g-ir-scanner
    # runtime. Different meson versions need different hacks.
    introspection_extra_sources = []
    introspection_extra_deps = []

    if meson.version().version_compare('>=0.59')
        introspection_extra_sources += custom_target('libtracker-sparql-modules-stamp',
            output: 'tracker-sparql-modules-stamp.c',
            command: ['touch', '@OUTPUT@'],
            depends: libtracker_sparql_modules)
    else
        introspection_extra_deps += libtracker_sparql_modules
    endif

    tracker_sparql_gir = gnome.generate_gir(libtracker_sparql,
        sources: [
            libtracker_sparql_sources,
            libtracker_sparql_public_headers,
            introspection_extra_sources,
            version_header,
            enum_types,
        ],
        dependencies: introspection_extra_deps,
        nsversion: tracker_api_version,
        namespace: 'Tracker',
        identifier_prefix: 'Tracker',
        symbol_prefix: 'tracker',
        includes : ['GLib-2.0', 'GObject-2.0', 'Gio-2.0' ],
        header: 'libtracker-sparql/tracker-sparql.h',
        link_with: libtracker_sparql,
        install: true,
        extra_args: [
            '-DTRACKER_COMPILATION',
        ],
        export_packages: 'tracker-sparql-' + tracker_api_version
    )

    tsparql_gir = gnome.generate_gir(libtracker_sparql,
        sources: [
            libtracker_sparql_sources,
            libtracker_sparql_public_headers,
            introspection_extra_sources,
            version_header,
            enum_types,
        ],
        dependencies: introspection_extra_deps,
        nsversion: tracker_api_version,
        namespace: 'Tsparql',
        identifier_prefix: 'Tracker',
        symbol_prefix: 'tracker',
        includes : ['GLib-2.0', 'GObject-2.0', 'Gio-2.0' ],
        header: 'tinysparql.h',
        link_with: libtracker_sparql,
        install: true,
        extra_args: [
            '-DTRACKER_COMPILATION',
        ],
        export_packages: 'tinysparql-' + tracker_api_version
    )

    if get_option('vapi').enabled() or get_option('vapi').auto()
        tracker_sparql_vapi = gnome.generate_vapi('tracker-sparql-' + tracker_api_version,
            sources: tracker_sparql_gir[0],
            install: true,
            packages: ['glib-2.0', 'gio-2.0', 'gobject-2.0'],
            metadata_dirs: meson.current_source_dir(),
        )

        tsparql_vapi = gnome.generate_vapi('tinysparql-' + tracker_api_version,
            sources: tsparql_gir[0],
            install: true,
            packages: ['glib-2.0', 'gio-2.0', 'gobject-2.0'],
            metadata_dirs: meson.current_source_dir(),
        )
    endif
endif

tracker_sparql_uninstalled_dir = meson.current_build_dir()

meson.add_install_script('symlink-libtracker-sparql.sh', get_option('libdir'), meson.current_build_dir())

meson.override_dependency('tinysparql-3.0', tracker_sparql_dep)

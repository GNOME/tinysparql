libtracker_sparql_modules = []

subdir('core')
subdir('bus')
subdir('direct')
subdir('remote')

version_header = configure_file(
    input: 'tracker-version-generated.h.meson.in',
    output: 'tracker-version-generated.h',
    configuration: conf)

enum_types = gnome.mkenums('tracker-sparql-enum-types',
    sources: ['tracker-notifier.h', 'tracker-connection.h', 'tracker-enums.h'],
    c_template: 'tracker-sparql-enum-types.c.template',
    h_template: 'tracker-sparql-enum-types.h.template',
    install_dir: join_paths(get_option('prefix'), get_option('includedir'), 'tracker-@0@'.format(tracker_api_version), 'libtracker-sparql'),
    install_header: true,
)

tracker_sparql_vapi = files('tracker-sparql.vapi')

libtracker_sparql_sources = files(
    'tracker-batch.c',
    'tracker-connection.c',
    'tracker-cursor.c',
    'tracker-deserializer.c',
    'tracker-deserializer-rdf.c',
    'tracker-deserializer-turtle.c',
    'tracker-deserializer-json.c',
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

sparqlinc = [include_directories('.'), meson.current_build_dir()]

install_headers(
    libtracker_sparql_public_headers,
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

libtracker_sparql_dependencies = [
    tracker_common_dep,
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

    c_args: [
	'-include', 'libtracker-sparql/tracker-private.h',
        '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
        '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
        '-DBUILDROOT="@0@"'.format(meson.build_root()),
    ] + tracker_c_args,

    dependencies: [libtracker_sparql_dependencies],
    gnu_symbol_visibility: 'hidden',
)

tracker_sparql_private_dep = declare_dependency(
    link_with: [libtracker_sparql_private],
    include_directories: [srcinc, include_directories('.')],
    sources: enum_types[1],
    dependencies: [glib, gio],
)

libtracker_sparql = library('tracker-sparql-' + tracker_api_version,
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
    include_directories: srcinc,
    dependencies: [glib, gio],
    sources: enum_types[1],
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
        ],
        dependencies: introspection_extra_deps,
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

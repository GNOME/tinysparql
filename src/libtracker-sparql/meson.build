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

sparqlinc = [include_directories('.'), meson.current_build_dir()]

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

libtracker_sparql_dependencies = [
    tracker_common_dep,
    tracker_sparql_vapi_dep,
    gmodule,
    libdl,
    json_glib,
    libxml2,
    tracker_data_dependencies
]

libtracker_sparql_private = static_library('tracker-sparql-private',
    enum_types,
    libtracker_sparql_c_sources,
    core_files,
    bus_files,
    direct_files,
    remote_files,
    '../libtracker-common/libtracker-common.vapi',
    'core/libtracker-data.vapi',
    'direct/tracker-direct.vapi',
    'tracker-backend.vala',
    'tracker-remote-module.c',
    tracker_gresources,

    c_args: [
	'-include', 'libtracker-sparql/tracker-private.h',
        '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
        '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
        '-DBUILDROOT="@0@"'.format(meson.build_root()),
    ] + tracker_c_args,

    vala_args: [
        '--debug',
        '--pkg', 'posix',
        # FIXME: Meson has code to add --target-glib automatically, but it
        # doesn't seem to work here.
        '--target-glib', glib_required,
    ],

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
)

libtracker_sparql_remote_c_sources = files(
    'remote/tracker-remote-statement.c',
    'tracker-serializer.c',
    'tracker-serializer-json.c',
    'tracker-serializer-trig.c',
    'tracker-serializer-turtle.c',
    'tracker-serializer-xml.c',
)

libtracker_sparql_remote_vala_sources = files(
    'remote/tracker-remote.vapi',
    'remote/tracker-json-cursor.vala',
    'remote/tracker-xml-cursor.vala',
    'remote/tracker-remote.vala',
)

if libsoup2.found()
    libtracker_remote_soup2 = shared_module('tracker-remote-soup2',
        libtracker_sparql_remote_c_sources, libtracker_sparql_remote_vala_sources,
        dependencies: [json_glib, libxml2, tracker_common_dep, tracker_sparql_dep,
                       tracker_sparql_vapi_dep, tracker_data_dep, libsoup2],
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
        name_suffix: 'so',
    )
    libtracker_sparql_modules += libtracker_remote_soup2
endif

if libsoup3.found()
    libtracker_remote_soup3 = shared_module('tracker-remote-soup3',
        libtracker_sparql_remote_c_sources, libtracker_sparql_remote_vala_sources,
        dependencies: [json_glib, libxml2, tracker_common_dep, tracker_sparql_dep,
                       tracker_sparql_vapi_dep, tracker_data_dep, libsoup3],
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
        name_suffix: 'so',
    )
    libtracker_sparql_modules += libtracker_remote_soup3
endif

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
            libtracker_sparql_c_sources,
            libtracker_sparql_c_public_headers,
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

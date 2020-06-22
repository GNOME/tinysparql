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
    'tracker-connection.c',
    'tracker-cursor.c',
    'tracker-endpoint.c',
    'tracker-endpoint-dbus.c',
    'tracker-namespace-manager.c',
    'tracker-notifier.c',
    'tracker-resource.c',
    'tracker-statement.c',
    'tracker-uri.c',
    'tracker-utils.c',
    'tracker-version.c',
)

libtracker_sparql_c_public_headers = files(
    'tracker-connection.h',
    'tracker-cursor.h',
    'tracker-endpoint.h',
    'tracker-endpoint-dbus.h',
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
    dependencies: [tracker_common_dep, json_glib],
    gnu_symbol_visibility: 'hidden',
)

sparqlinc = [include_directories('.'), meson.current_build_dir()]

tracker_sparql_intermediate_dep = declare_dependency(
    link_with: [libtracker_sparql_intermediate],
    include_directories: [srcinc, include_directories('.')],
    dependencies: [ tracker_sparql_vapi_dep ],
)

install_headers(
    libtracker_sparql_c_public_headers,
    'tracker-ontologies.h',
    'tracker-sparql.h',
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

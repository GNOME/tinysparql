libtracker_sparql = library('tracker-sparql-' + tracker_api_version,
    'tracker-backend.vala',
    install: true,
    install_rpath: tracker_internal_libs_dir,
    dependencies: [tracker_sparql_intermediate_dep, tracker_sparql_remote_dep, tracker_sparql_bus_dep, tracker_sparql_direct_dep],
)

tracker_sparql_dep = declare_dependency(
    link_with: [libtracker_sparql],
    include_directories: srcinc,
    dependencies: [tracker_sparql_intermediate_dep],
)

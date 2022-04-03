direct_files = files(
    'tracker-direct.c',
    'tracker-direct-batch.c',
    'tracker-direct-statement.c',
)

libtracker_direct = static_library('tracker-direct',
    direct_files,
    c_args: tracker_c_args + [
	'-include', 'libtracker-sparql/tracker-private.h',
    ],
    dependencies: [ glib, gio, tracker_data_dep, tracker_sparql_intermediate_dep ],
    include_directories: [commoninc, configinc, srcinc],
    gnu_symbol_visibility: 'hidden',
)

tracker_sparql_direct_dep = declare_dependency(
    link_with: libtracker_direct,
    include_directories: include_directories('.')
)

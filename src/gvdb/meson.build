gvdb_dependencies = [glib]

libgvdb = static_library('gvdb',
    'gvdb-builder.c',
    'gvdb-reader.c',
    dependencies: gvdb_dependencies,
    pic: true,
)

gvdb_dep = declare_dependency(
    link_with: libgvdb,
    dependencies: gvdb_dependencies,
    include_directories: include_directories('..'),
)

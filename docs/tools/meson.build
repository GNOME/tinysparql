# These tools are used for generating the ontologies documentation.

doctool_gresources = gnome.compile_resources('tracker_doctool_gresources', 'dsc-ontology.gresource.xml')

doctool_files = [
    'tracker-docgen-md.c',
    'tracker-main.c',
    'tracker-ontology-model.c',
    'tracker-utils.c',
]

tracker_docgen = executable('tracker-docgen',
    doctool_files,
    doctool_gresources,
    # Prefer RPATH over RUNPATH so the executable
    # always finds the uninstalled library.
    link_args: ['-Wl,--disable-new-dtags'],
    dependencies: [tracker_sparql_dep])

# These tools are used for generating the ontologies documentation.

doctool_gresources = gnome.compile_resources('tracker_doctool_gresources', 'dsc-ontology.gresource.xml')

doctool_files = [
    'tracker-docgen-md.c',
    'tracker-docgen-xml.c',
    'tracker-main.c',
    'tracker-ontology-model.c',
    'tracker-utils.c',
]

tracker_docgen = executable('tracker-docgen',
    doctool_files,
    doctool_gresources,
    dependencies: [tracker_sparql_dep])

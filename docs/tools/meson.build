# These tools are used for generating the ontologies documentation.

doctool_gresources = gnome.compile_resources('tracker_doctool_gresources', 'dsc-ontology.gresource.xml')

doctool_files = [
    'tracker-docgen-xml.c',
    'tracker-ontology-model.c',
    'tracker-utils.c',
    'ttl2xml.c',
]

ttl2xml = executable('ttl2xml',
    doctool_files,
    doctool_gresources,
    dependencies: [tracker_sparql_dep])

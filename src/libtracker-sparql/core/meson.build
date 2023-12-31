tracker_data_enums = gnome.mkenums_simple('tracker-data-enum-types',
    sources: 'tracker-db-interface-sqlite.h',
    body_prefix: '#include "config.h"',
)

tracker_data_dependencies = [
    tracker_common_dep, sqlite,
]

core_files = files(
    'tracker-class.c',
    'tracker-collation.c',
    'tracker-data-manager.c',
    'tracker-data-query.c',
    'tracker-data-update.c',
    'tracker-db-interface.c',
    'tracker-db-interface-sqlite.c',
    'tracker-db-manager.c',
    'tracker-fts-tokenizer.c',
    'tracker-namespace.c',
    'tracker-ontology.c',
    'tracker-ontologies.c',
    'tracker-ontologies-diff.c',
    'tracker-ontologies-introspect.c',
    'tracker-ontologies-rdf.c',
    'tracker-property.c',
    'tracker-rowid.c',
    'tracker-string-builder.c',
    'tracker-sparql-parser.c',
    'tracker-sparql-types.c',
    'tracker-sparql.c',
    'tracker-uuid.c',
    'tracker-vtab-service.c',
    'tracker-vtab-triples.c',
)

core_files += [
    tracker_data_enums[0],
    tracker_data_enums[1],
]

core_incs = include_directories('.')

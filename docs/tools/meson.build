# These tools are used for generating the ontologies documentation.

ttl_loader_files = [
    'ttl_loader.c',
    'ttl_model.c',
    'ttl_sgml.c',
]

ttl2sgml = executable('ttl2sgml',
    ttl_loader_files, 'ttl2sgml.c', 'ttlresource2sgml.c',
    dependencies: [tracker_data_dep, tracker_sparql_dep])

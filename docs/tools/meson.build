# These tools are used for generating the ontologies documentation.

ttl_loader_files = [
    'ttl_loader.c',
    'ttl_model.c',
    'ttl_sgml.c',
    'qname.c',
]

ttl2sgml = executable('ttl2sgml',
    ttl_loader_files, 'ttl2sgml.c',
    dependencies: [tracker_data_dep, tracker_sparql_dep],
    c_args: ['-DTRACKER_COMPILATION'])

ttlresource2sgml = executable('ttlresource2sgml',
    ttl_loader_files, 'ttlresource2sgml.c',
    dependencies: [tracker_data_dep, tracker_sparql_dep],
    c_args: ['-DTRACKER_COMPILATION'])

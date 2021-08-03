documents_with_examples = [
  [ 'examples.md.in', 'examples.md' ],
]
examples = []

foreach doc : documents_with_examples
  examples += custom_target(
    'examples-generate',
    input: doc[0],
    output: doc[1],
    command: [
      'embed-files.py',
      '@INPUT@',
      '@OUTPUT@',
    ],
    depend_files: [ doc[0] ],
    build_by_default: true)
endforeach

base_ontology_docs = custom_target('ontology-docgen',
    output: ['dc-ontology.md'],
    command: [tracker_docgen,
              '--md',
              '-d', join_paths(source_root, 'src/ontologies/'),
              '-o', join_paths(meson.current_build_dir())],
    depends: tracker_docgen,
    depend_files: [base_ontology],
    build_by_default: true,
)

content = [
  'overview.md',
  'ontologies.md',
  'limits.md',
  'performance.md',
  'sparql-and-tracker.md',
  'sparql-functions.md',
  'migrating-2to3.md',
  'tutorial.md',
]

generated_content = [
  'examples.md',
  'xsd-ontology.md',
  'dc-ontology.md',
  'rdf-ontology.md',
  'rdfs-ontology.md',
  'nrl-ontology.md',
]

required_hotdoc_extensions = [
  'gi-extension',
  'devhelp-extension',
  'syntax-highlighting-extension',
]

foreach ext: required_hotdoc_extensions
  if not hotdoc.has_extensions(ext)
    error('Documentation enabled but HotDoc extension "@0@" is missing'.format(ext))
  endif
endforeach

hotdoc.generate_doc('tracker',
  project_version: tracker_version,
  languages: [ 'c', 'python', 'javascript' ],
  gi_c_sources: [libtracker_sparql_c_sources, libtracker_sparql_c_public_headers, libtracker_sparql_remote_c_sources],
  gi_sources: [tracker_sparql_gir[0].full_path()],
  sitemap: 'sitemap.txt',
  index: 'index.md',
  gi_index: 'gi-index.md',
  gi_smart_index: true,
  gi_c_source_roots: [sparqlinc],
  dependencies: [tracker_sparql_dep, glib, gobject],
  extra_assets: [join_paths(meson.current_source_dir(), 'images')],
  syntax_highlighting_activate: true,
  devhelp_activate: true,
  install: true,
)

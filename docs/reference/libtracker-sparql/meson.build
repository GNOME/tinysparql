fs = import('fs')

if fs.exists('doc')
  # Special case for tarballs, install the pre-generated docs
  install_subdir('doc',
    install_dir: datadir)
  subdir_done()
endif

gidocgen_dep = dependency('gi-docgen', fallback: ['gi-docgen', 'dummy_dep'])
gidocgen = find_program('gi-docgen')

ontology_introductions = [
  'nie-introduction.md',
  'nmm-introduction.md',
  'nco-introduction.md',
  'mfo-introduction.md',
]

base_ontology_docs = custom_target('ontology-docgen',
    output: ['dc-ontology.md'],
    command: [tracker_docgen,
              '--md',
              '--ontology-dir', meson.current_build_dir(), # Directory without ontology files
              '--output-dir', meson.current_build_dir(),
              '--introduction-dir', meson.current_source_dir(),
              '--ontology-description-dir', join_paths(source_root, 'src/ontologies/')],
    depends: tracker_docgen,
    depend_files: [base_ontology, ontology_introductions],
    build_by_default: true,
)

nepomuk_ontology_docs = custom_target('nepomuk-docgen',
    output: ['nie-ontology.md'],
    command: [tracker_docgen,
              '--md',
              '--ontology-dir', join_paths(source_root, 'src/ontologies/nepomuk'),
              '--output-dir', meson.current_build_dir(),
              '--introduction-dir', meson.current_source_dir()],
    depends: tracker_docgen,
    depend_files: [nepomuk, ontology_introductions],
    build_by_default: true,
)

generated_content = [
  'xsd-ontology.md',
  'rdf-ontology.md',
  'rdfs-ontology.md',
  'nrl-ontology.md',
  'dc-ontology.md',

  'nie-ontology.md',
  'nao-ontology.md',
  'nco-ontology.md',
  'nfo-ontology.md',
  'nmm-ontology.md',
  'mfo-ontology.md',
  'tracker-ontology.md',
  'slo-ontology.md',
  'osinfo-ontology.md',
]

content = [
  'overview.md',
  'examples.md',
  'tutorial.md',
  'ontologies.md',
  'sparql-functions.md',
  'sparql-and-tracker.md',
  'limits.md',
  'performance.md',
  'security.md',
  'migrating-2to3.md',
]

# The TOML gi-docgen configuration wants a list of quoted file names.
content_quoted = []
foreach c : content + generated_content
  content_quoted += '"@0@"'.format(c)
endforeach

gidocgen_conf = configuration_data()
gidocgen_conf.set('version', meson.project_version())
gidocgen_conf.set('content', ','.join(content_quoted))

gidocgen_toml = configure_file(
  input: 'tracker-sparql.toml.in',
  output: 'tracker-sparql.toml',
  configuration: gidocgen_conf)

reference = custom_target(
  'docgen',
  input: [ gidocgen_toml, tracker_sparql_gir[0] ],
  output: 'doc',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '@INPUT1@',
  ],
  depends: [tracker_sparql_gir[0], base_ontology_docs, nepomuk_ontology_docs],
  depend_files: content)

docs_name = 'Tracker-3.0'
meson.add_install_script('install-devhelp.sh', docs_name, reference)
meson.add_dist_script('dist-docs.sh', docs_name, reference.full_path())

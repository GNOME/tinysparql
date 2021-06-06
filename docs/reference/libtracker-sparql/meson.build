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
  'examples.md',
  'limits.md',
  'performance.md',
  'sparql-and-tracker.md',
  'sparql-functions.md',
  'migrating-2to3.md',
  'tutorial.md',
]

generated_content = [
  'xsd-ontology.md',
  'dc-ontology.md',
  'rdf-ontology.md',
  'rdfs-ontology.md',
  'nrl-ontology.md',
]

# The TOML gi-docgen configuration wants a list of quoted file names.
_quoted = []
foreach c : content + generated_content
  _quoted += '"@0@"'.format(c)
endforeach

gidocgen_conf = configuration_data()
gidocgen_conf.set('version', meson.project_version())
gidocgen_conf.set('content', ','.join(_quoted))
gidocgen_toml = configure_file(input: 'tracker-sparql.toml.in', output: 'tracker-sparql.toml', configuration: gidocgen_conf)

custom_target(
  'docgen',
  input: [ gidocgen_toml, tracker_sparql_gir[0] ],
  output: 'docs',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    #'--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    #'--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '--content-dir=@0@'.format(icon_images_base),
    '@INPUT1@',
  ],
  depends: [tracker_sparql_gir[0], base_ontology_docs],
  depend_files: [ content ],
  build_by_default: true)

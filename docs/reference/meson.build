fs = import('fs')

if fs.exists('doc')
  # Special case for tarballs, install the pre-generated docs
  install_subdir('doc',
    install_dir: datadir)
  subdir_done()
endif

gidocgen_dep = dependency('gi-docgen', fallback: ['gi-docgen', 'dummy_dep'])
gidocgen = find_program('gi-docgen')
graphviz_dot = find_program('dot')
xmlto = find_program('xmlto', required: false)

ontology_introductions = [
  'nie-introduction.md',
  'nmm-introduction.md',
  'nco-introduction.md',
  'mfo-introduction.md',
]

base_ontology_docs = custom_target('ontology-docgen',
    output: ['dc-ontology.md.in'],
    command: [tracker_docgen,
              '--md',
              '--ontology-dir', meson.current_build_dir(), # Directory without ontology files
              '--output-dir', meson.current_build_dir(),
              '--introduction-dir', meson.current_source_dir(),
              '--ontology-description-dir', join_paths(source_root, 'src/ontologies/')],
    depends: [tracker_docgen, libtracker_parser],
    depend_files: [base_ontology, ontology_introductions],
)

nepomuk_ontology_docs = custom_target('nepomuk-docgen',
    output: ['nie-ontology.md.in'],
    command: [tracker_docgen,
              '--md',
              '--ontology-dir', join_paths(source_root, 'src/ontologies/nepomuk'),
              '--output-dir', meson.current_build_dir(),
              '--introduction-dir', meson.current_source_dir()],
    depends: [tracker_docgen, libtracker_parser],
    depend_files: [nepomuk, ontology_introductions],
)

generate_images = custom_target(
  'doc-images',
  output: 'rdfs:Resource-hierarchy.svg',
  command: [
    'generate-svgs.sh',
    graphviz_dot,
    meson.current_build_dir(),
  ],
  depends: [base_ontology_docs, nepomuk_ontology_docs])

generated_ontology_files = [
  'xsd-ontology.md.in',
  'rdf-ontology.md.in',
  'rdfs-ontology.md.in',
  'nrl-ontology.md.in',
  'dc-ontology.md.in',

  'nie-ontology.md.in',
  'nao-ontology.md.in',
  'nco-ontology.md.in',
  'nfo-ontology.md.in',
  'nmm-ontology.md.in',
  'mfo-ontology.md.in',
  'tracker-ontology.md.in',
  'slo-ontology.md.in',
  'osinfo-ontology.md.in',
]

generated_targets = []
generated_ontology_content = []

foreach doc : generated_ontology_files
  output_file = doc.replace('.in', '')
  generated_ontology_content += output_file
  generated_targets += custom_target(
    output_file,
    output: output_file,
    command: [
      'embed-files.py',
      meson.current_build_dir() / doc,
      meson.current_build_dir() / output_file,
      meson.current_source_dir(),
    ],
    depends: [base_ontology_docs, nepomuk_ontology_docs, generate_images])
endforeach

generated_content_files = [
  'overview.md.in',
  'examples.md.in',
  'tutorial.md.in',
  'sandboxing.md.in',
]

generated_content = []

foreach doc : generated_content_files
  output_file = doc.replace('.in', '')
  generated_content += output_file
  generated_targets += custom_target(
    output_file,
    input: doc,
    output: output_file,
    command: [
      'embed-files.py',
      '@INPUT@',
      '@OUTPUT@',
    ])
endforeach

content = [
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
foreach c : generated_content + content + generated_ontology_content
  content_quoted += '"@0@"'.format(fs.name(c))
endforeach

if xmlto.found() and get_option('man')
  cli_reference = custom_target(
    'commandline.md',
    input: 'commandline.md.in',
    output: 'commandline.md',
    command: [
      'generate-cli-reference.py',
      '--template', '@INPUT@',
      '--output', '@OUTPUT@',
      '--man-pages', manpage_files,
    ]
  )

  generated_targets += cli_reference
  content_quoted += '"@0@"'.format(fs.name(cli_reference.full_path()))
endif

gidocgen_conf = configuration_data()
gidocgen_conf.set('version', meson.project_version())
gidocgen_conf.set('content', ','.join(content_quoted))

gidocgen_toml = configure_file(
  input: 'tracker-sparql.toml.in',
  output: 'tracker-sparql.toml',
  configuration: gidocgen_conf)

reference = custom_target(
  'docgen',
  input: [ gidocgen_toml, tsparql_gir[0] ],
  output: 'doc',
  command: [
    'docgen-wrapper.sh',
    '@OUTPUT@',
    gidocgen,
    'generate',
    '--quiet',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '@INPUT1@',
  ],
  depends: [tracker_sparql_gir[0], generated_targets],
  depend_files: [content],
  install: true,
  install_dir: datadir,
  install_tag: 'doc')

meson.add_dist_script('dist-docs.sh', reference.full_path())

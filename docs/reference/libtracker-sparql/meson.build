content = [
  'overview.md',
  'ontologies.md',
  'examples.md',
  'limits.md',
  'performance.md',
  'sparql-and-tracker.md',
  'sparql-functions.md',
  'tutorial.md',
]

# The TOML gi-docgen configuration wants a list of quoted file names.
_quoted = []
foreach c : content
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
    '@INPUT1@',
  ],
  depends: tracker_sparql_gir[0],
  depend_files: [ content ],
  build_by_default: true)

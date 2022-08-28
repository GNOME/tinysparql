fs = import('fs')

if fs.exists('devhelp')
  # Special case for tarballs, install the pre-generated docs
  install_subdir('devhelp/books',
    install_dir: join_paths(datadir, 'devhelp'))
  subdir_done()
endif

hotdoc = import('hotdoc')

base_ontology_docs = custom_target('ontology-docgen',
    output: ['dc-ontology.md'],
    command: [tracker_docgen,
              '--md',
              '--ontology-dir', meson.current_build_dir(), # Directory without ontology files
              '--output-dir', meson.current_build_dir(),
              '--introduction-dir', meson.current_source_dir(),
              '--ontology-description-dir', join_paths(source_root, 'src/ontologies/')],
    depends: tracker_docgen,
    depend_files: [base_ontology],
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
    depend_files: [base_ontology],
    build_by_default: true,
)

content = [
  'overview.md',
  'examples.md',
  'ontologies.md',
  'limits.md',
  'performance.md',
  'sparql-and-tracker.md',
  'sparql-functions.md',
  'migrating-2to3.md',
  'tutorial.md',
  'security.md',
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

docs_name = 'Tracker'
hotdoc.generate_doc(docs_name,
  project_version: tracker_version,
  languages: [ 'c', 'python', 'javascript' ],
  gi_c_sources: [libtracker_sparql_sources, libtracker_sparql_public_headers],
  gi_sources: [tracker_sparql_gir[0].full_path()],
  sitemap: 'sitemap.txt',
  index: 'index.md',
  gi_index: 'gi-index.md',
  gi_smart_index: true,
  gi_c_source_roots: [sparqlinc],
  dependencies: [tracker_sparql_dep, base_ontology_docs, nepomuk_ontology_docs],
  extra_assets: [join_paths(meson.current_source_dir(), 'images')],
  html_theme: 'https://github.com/hotdoc/hotdoc_lumen_theme/releases/download/0.13.2/hotdoc_lumen_theme-0.13.2.tar.xz?sha256=5721189b7e985f27381ee20137f4a9003049a70a75ab1221a69fd04d27e752bc',
  html_extra_theme: join_paths(meson.current_source_dir(), 'theme-extra'),
  syntax_highlighting_activate: true,
  devhelp_activate: true,
  devhelp_online: 'https://gnome.pages.gitlab.gnome.org/tracker/docs/developer/',
  install: true,
)

meson.add_install_script('install-devhelp.sh', docs_name)
meson.add_dist_script('dist-docs.sh', docs_name)

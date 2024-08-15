manpages = [
    ['tinysparql-endpoint', 1, true],
    ['tinysparql-export', 1, true],
    ['tinysparql-import', 1, true],
    ['tinysparql-introspect', 1, true],
    ['tinysparql-query', 1, true],
    ['tinysparql-sql', 1, true],
    ['tinysparql-xdg-portal-3', 1, false],
]

manpage_files = []

foreach m : manpages
  name = m[0]
  section = m[1]
  documentation = m[2]

  manpage = '@0@.@1@'.format(name, section)
  if documentation
    manpage_files += join_paths(meson.current_source_dir(), manpage + '.txt')
  endif

  custom_target(manpage,
    command: [a2x,
              '--attribute=author=The TinySPARQL developers',
              '--attribute=manversion=@0@'.format(meson.project_version()),
              '--attribute=manmanual=TinySPARQL manual',
              '--doctype=manpage',
              '--format=manpage',
              '--no-xmllint',
              '--xsltproc-opts=' + ' '.join([
                '--stringparam', 'man.authors.section.enabled', '0',
              ]),
              '-D', '@OUTDIR@',
              '@INPUT@'],
    input: manpage + '.txt',
    output: manpage,
    install: true,
    install_dir: get_option('mandir') / 'man@0@'.format(section),
  )
endforeach

manpages = [
    ['tracker3-endpoint', 1],
    ['tracker3-export', 1],
    ['tracker3-import', 1],
    ['tracker3-sparql', 1],
    ['tracker3-sql', 1],
    ['tracker-xdg-portal-3', 1],
]

foreach m : manpages
  name = m[0]
  section = m[1]

  manpage = '@0@.@1@'.format(name, section)

  custom_target(manpage,
    command: [a2x,
              '--attribute=author=The Tracker developers',
              '--attribute=manversion=@0@'.format(meson.project_version()),
              '--attribute=manmanual=Tracker manual',
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

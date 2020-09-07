manpages = [
    ['endpoint', 'tracker3-endpoint', 1],
    ['export', 'tracker3-export', 1],
    ['import', 'tracker3-import', 1],
    ['sparql', 'tracker3-sparql', 1],
    ['sql', 'tracker3-sql', 1],
    ['xdg-portal', 'tracker-xdg-portal-3', 1],
]

foreach m : manpages
  src = m[0]
  dst = m[1]
  section = m[2]

  manpage_src = 'tracker-@0@.@1@.txt'.format(src, section)
  manpage_xml = '@0@.@1@.xml'.format(dst, section)
  manpage = '@0@.@1@'.format(dst, section)

  xml = custom_target(manpage_xml,
    command: [asciidoc,
              '--attribute=author=The Tracker developers',
              '--attribute=manversion=@0@'.format(meson.project_version()),
              '--attribute=manmanual=Tracker manual',
              '--backend', 'docbook',
              '--doctype', 'manpage',
              '--out-file', '@OUTPUT@', '@INPUT@'],
    input: manpage_src,
    output: manpage_xml
  )

  custom_target(manpage,
    command: [xsltproc,
              '--output', '@OUTPUT@',
              '--stringparam', 'man.authors.section.enabled', '0',
              '/etc/asciidoc/docbook-xsl/manpage.xsl', '@INPUT@'],
    input: xml,
    output: manpage,
    install: true,
    install_dir: get_option('mandir') / 'man@0@'.format(section),
  )
endforeach

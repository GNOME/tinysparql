version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-sparql',
    src_dir: 'src/libtracker-sparql',
    main_sgml: 'libtracker-sparql-docs.sgml',
    content_files: ['overview.sgml', 'examples.sgml'],
    install: true)

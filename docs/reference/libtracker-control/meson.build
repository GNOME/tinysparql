version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-control',
    src_dir: 'src/libtracker-control',
    main_sgml: 'libtracker-control-docs.sgml',
    install: true)

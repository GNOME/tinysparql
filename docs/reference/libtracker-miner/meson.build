version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-miner',
    src_dir: 'src/libtracker-miner',
    main_sgml: 'libtracker-miner-docs.sgml',
    content_files: ['overview.sgml', 'migrating-1to2.xml'],
    install: true)

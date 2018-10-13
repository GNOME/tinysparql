version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-miner',
    src_dir: minerinc,
    main_sgml: 'libtracker-miner-docs.sgml',
    content_files: ['overview.sgml', 'migrating-1to2.xml'],
    dependencies: tracker_miner_dep,
    gobject_typesfile: 'libtracker-miner.types',
    fixxref_args: fixxref_args,
    install: true)

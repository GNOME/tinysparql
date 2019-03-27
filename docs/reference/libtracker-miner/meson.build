version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-miner',
    src_dir: minerinc,
    main_xml: 'libtracker-miner-docs.xml',
    content_files: ['overview.xml', 'migrating-1to2.xml'],
    dependencies: tracker_miner_dep,
    scan_args: ['--rebuild-sections'],
    fixxref_args: fixxref_args,
    install: true)

version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-control',
    src_dir: join_paths(meson.current_source_dir(), '..', '..', '..', 'src', 'libtracker-control'),
    main_sgml: 'libtracker-control-docs.sgml',
    content_files: ['migrating-1to2.xml'],
    install: true)

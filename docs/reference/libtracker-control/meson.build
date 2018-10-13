version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libtracker-control',
    src_dir: controlinc,
    main_sgml: 'libtracker-control-docs.sgml',
    content_files: ['migrating-1to2.xml'],
    dependencies: tracker_control_dep,
    gobject_typesfile: 'libtracker-control.types',
    fixxref_args: fixxref_args,
    install: true)

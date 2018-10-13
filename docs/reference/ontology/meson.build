version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

generated = custom_target('ontology-doc-generated',
    output: 'gen-doc.stamp',
    command: [ttl2sgml,
              '-d', join_paths(source_root, 'src/ontologies/nepomuk'),
              '-o', join_paths(meson.current_build_dir(), 'xml/'),
              '-e', meson.current_source_dir()],
    depends: ttl2sgml,
    depend_files: nepomuk,
    build_by_default: true,
)

gnome.gtkdoc('ontology',
    src_dir: include_directories('.'),
    main_sgml: 'ontology-docs.sgml',
    install: true)

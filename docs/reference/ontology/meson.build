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
    # FIXME: this shouldn't be necessary, but currently the 'dependencies'
    # parameter to gnome.gtkdoc() doesn't actually trigger building of custom
    # targets.
    build_always: true
)

gnome.gtkdoc('ontology',
    src_dir: include_directories('.'),
    main_sgml: 'ontology-docs.sgml',
    # FIXME: passing 'generated' here has no effect; the gtkdoc support in
    # Meson actually only uses this parameter for building and linking the
    # GObject Introspection scanner right now.
    dependencies: generated,
    install: true)

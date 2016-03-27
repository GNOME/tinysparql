version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)


#for f in `find $ONTOLOGIES_DATA_DIR -name "*.description"` ; do
#    # ../../src/ontologies/XX-aaa.description -> PREFIX=aaa
#    TMPNAME=${f%.description}
#    PREFIX=${TMPNAME#*-}
#    echo "- Generating $PREFIX documentation"
#
#    $TTL2SGML -d $f -o $BUILD_DIR/$PREFIX-ontology.xml \
#        -e $ONTOLOGIES_INFO_DIR/$PREFIX/explanation.xml


gen_doc = meson.source_root() + '/docs/tools/gen-doc.sh'
generated = custom_target('ontology-doc-generated',
    output: 'gen-doc.stamp',
    command: [gen_doc, ttl2sgml, ttlresource2sgml,
              meson.source_root() + '/src/ontologies',
              meson.source_root() + '/docs/ontologies',
              join_paths(meson.current_build_dir(), 'xml/')],
    depends: [ttl2sgml, ttlresource2sgml],
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

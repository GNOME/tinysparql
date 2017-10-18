if sqlite3_has_builtin_fts5
    libtracker_fts_fts5 = []
else
    libtracker_fts_fts5 = files('fts5.c')
endif

libtracker_fts_dependencies = [sqlite]

libtracker_fts = static_library('libtracker-fts',
    'tracker-fts.c',
    'tracker-fts-config.c',
    'tracker-fts-tokenizer.c',
    libtracker_fts_fts5,
    dependencies: [tracker_common_dep, tracker_sparql_intermediate_dep],
    c_args: tracker_c_args
)

tracker_fts_dep = declare_dependency(
    link_with: libtracker_fts,
    dependencies: libtracker_fts_dependencies
)

custom_target('libtracker-fts-settings-schema',
    input: 'org.freedesktop.Tracker.FTS.gschema.xml.in',
    output: 'org.freedesktop.Tracker.FTS.gschema.xml',
    command: ['intltool-merge', '--quiet', '--xml-style', '--utf8', po_subdir, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: gsettings_schema_dir)

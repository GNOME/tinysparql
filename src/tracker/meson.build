commands_dir = get_option('bindir')
command_metadata_dir = get_option('prefix') / get_option('datadir') / 'tracker3' / 'commands'
po_dir = join_paths(meson.project_source_root(), 'po')

sparql_gresource = gnome.compile_resources('sparql_gresources', 'sparql-queries.gresource.xml')

commands = [
    { 'name': 'endpoint', 'sources': 'tracker-endpoint.c', 'extra_deps': [avahi_glib, avahi_client] },
    # We link against the private tracker-sparql library for the `--2to3` export mode.
    { 'name': 'export', 'sources': 'tracker-export.c', 'extra_deps': [tracker_sparql_private_dep] },
    { 'name': 'help', 'sources': 'tracker-help.c', 'extra_deps': [] },
    { 'name': 'import', 'sources': 'tracker-import.c', 'extra_deps': [] },
    { 'name': 'sparql', 'sources': ['tracker-sparql.c', sparql_gresource], 'extra_deps': [] },
    { 'name': 'sql', 'sources': 'tracker-sql.c', 'extra_deps': [tracker_sparql_private_dep] },
]

main_command_name = 'tracker@0@'.format(tracker_api_major)
main_command_sources = [
    'tracker-main.c',
]

executable(main_command_name, main_command_sources,
    c_args: tracker_c_args + [
        '-DCLI_METADATA_DIR="@0@"'.format(command_metadata_dir),
        '-DMANDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'man')),
        '-DBINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
    ],
    install: true,
    install_rpath: tracker_internal_libs_dir,
    dependencies: [tracker_common_dep, tracker_sparql_dep, gio_unix],
    include_directories: [commoninc, configinc, srcinc],
)

foreach c: commands
    command_name = 'tracker@0@-@1@'.format(tracker_api_major, c['name'])
    command_sources = c['sources']

    executable(command_name, command_sources,
        c_args: tracker_c_args + [
            '-DMAIN_COMMAND_NAME="@0@"'.format(main_command_name),
            '-DMANDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'man')),
            '-DBINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
        ],
        install: true,
        install_dir: get_option('bindir'),
        dependencies: [tracker_common_dep, tracker_sparql_dep] + c['extra_deps'],
        include_directories: [commoninc, configinc, srcinc],
    )

    command_desktop_file = 'tracker-@0@.desktop'.format(c['name'])
    command_desktop = i18n.merge_file(
        input: command_desktop_file + '.in',
        output: command_desktop_file,
        install: true,
        install_dir: command_metadata_dir,
        po_dir: po_dir,
        type: 'desktop'
    )
endforeach

if get_option('bash_completion')
    install_data(
        sources: 'bash-completion/tracker',
        install_dir: bash_completion_dir,
        rename: main_command_name)
endif

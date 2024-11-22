tracker_uninstalled_cli_dir = meson.current_build_dir()

commands_dir = get_option('bindir')
command_metadata_dir = get_option('prefix') / get_option('datadir') / 'tinysparql-3.0' / 'commands'
po_dir = join_paths(meson.project_source_root(), 'po')

sparql_gresource = gnome.compile_resources('sparql_gresources', 'sparql-queries.gresource.xml')

main_command_name = 'tinysparql'
main_command_sources = [
    'tracker-help.c',
    'tracker-endpoint.c',
    'tracker-export.c',
    'tracker-import.c',
    'tracker-introspect.c',
    'tracker-main.c',
    'tracker-query.c',
    'tracker-webide.c',
    sparql_gresource,
]

executable(main_command_name, main_command_sources, web_ide_gresources,
    c_args: tracker_c_args + [
        '-DCLI_METADATA_DIR="@0@"'.format(command_metadata_dir),
        '-DMANDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'man')),
        '-DBINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
        '-DMAIN_COMMAND_NAME="@0@"'.format(main_command_name),
        '-DBUILDROOT="@0@"'.format(meson.global_build_root()),
        '-DLIBEXECDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('libexecdir'))),
    ],
    install: true,
    install_rpath: tracker_internal_libs_dir,
    dependencies: [tracker_common_dep, tracker_sparql_dep, web_ide_dep, gio_unix, avahi_glib, avahi_client],
    include_directories: [commoninc, configinc, srcinc],
)

executable('tinysparql-sql', 'tracker-sql.c',
    c_args: tracker_c_args + [
        '-DMANDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'man')),
        '-DBINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
        '-DMAIN_COMMAND_NAME="@0@"'.format(main_command_name),
    ],
    install: true,
    install_dir: get_option('libexecdir'),
    dependencies: [tracker_sparql_private_dep],
    include_directories: [commoninc, configinc, srcinc],
)

if get_option('bash_completion')
    install_data(
        sources: 'bash-completion/tinysparql',
        install_dir: bash_completion_dir,
        rename: main_command_name)
endif

cli_devenv = environment()
cli_devenv.set('TRACKER_CLI_DIR', meson.current_build_dir())
meson.add_devenv(cli_devenv)

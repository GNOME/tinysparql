modules = [
    'endpoint',
    'export',
    'help',
    'import',
    'sparql',
    'sql',
]

sources = [
    'tracker-main.c',
]

foreach m: modules
    sources += 'tracker-@0@.c'.format(m)
endforeach

command_name = 'tracker@0@'.format(tracker_api_major)
subcommands_dir = join_paths(get_option('prefix'), get_option('libexecdir'), tracker_versioned_name)

executable(command_name, sources,
    c_args: tracker_c_args + [
        '-DCOMMANDNAME="@0@"'.format(command_name),
        '-DSUBCOMMANDSDIR="@0@"'.format(subcommands_dir),
        '-DMANDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('datadir'), 'man')),
        '-DBINDIR="@0@"'.format(join_paths(get_option('prefix'), get_option('bindir'))),
    ],
    install: true,
    install_rpath: tracker_internal_libs_dir,
    dependencies: [tracker_common_dep, tracker_sparql_dep, tracker_sparql_private_dep, gio_unix],
    include_directories: [commoninc, configinc, srcinc],
)

meson.add_install_script('make-subcommand-links.sh',
    [ '@0@'.format(join_paths(get_option('prefix'), get_option('bindir'), command_name)),
      '@0@'.format(subcommands_dir),
    ] + modules)

if get_option('bash_completion')
    install_data(
        sources: 'bash-completion/tracker',
        install_dir: bash_completion_dir,
	rename: command_name)
endif

run_command('make-uninstalled-subcommand-links.sh', modules,
            check: true)

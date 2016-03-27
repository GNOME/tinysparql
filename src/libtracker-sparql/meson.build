enums_c = gnome.mkenums('tracker-sparql-enum-types',
    sources: 'tracker-notifier.h',
    c_template: 'tracker-sparql-enum-types.c.template',
)
enums_h = gnome.mkenums('tracker-sparql-enum-types',
    sources: 'tracker-notifier.h',
    h_template: 'tracker-sparql-enum-types.h.template',
)

tracker_sparql_intermediate_dependencies = [uuid]

libtracker_sparql_intermediate = static_library('tracker-sparql',
    enums_c, enums_h,
    'tracker-namespace.vala',
    'tracker-builder.vala',
    'tracker-connection.vala',
    'tracker-cursor.vala',
    'tracker-utils.vala',
    'tracker-namespace-manager.c',
    'tracker-notifier.c',
    'tracker-resource.c',
    'tracker-uri.c',
    'tracker-version.c',
    vala_header: 'tracker-generated-no-checks.h',
    c_args: [ '-DTRACKER_COMPILATION', ],
    # FIXME: getting Vala to output the GIR means that the symbols from the
    # C files aren't included. This problem seems to affect the Autotools
    # build system as well.
    # FIXME: we also can't depend on this in the TrackerMiner GIR, which
    # probably breaks stuff
    vala_gir: 'Tracker-' + tracker_api_version + '.gir',
    vala_args: [
        '--debug',
        '--includedir', 'libtracker-sparql',
        '--pkg', 'gio-2.0', '--pkg', 'posix',
        # FIXME: Meson has code to add --target-glib automatically, but it
        # doesn't seem to work here.
        '--target-glib', glib_required,
    ],
    # This doesn't depend on tracker_common_dep because of
    # https://github.com/mesonbuild/meson/issues/671
    dependencies: [glib, gio] + tracker_sparql_intermediate_dependencies,
    include_directories: [commoninc, configinc, srcinc],
    link_with: [libtracker_common],
)

# We must postprocess the generated Vala header to have only-include-once guards
tracker_sparql_generated_header = custom_target('tracker-sparql-generated-header',
    # FIXME: can't list tracker-generated-no-checks as an input, even though it
    # is, because you'll see this:
    #
    #     ninja: error:
    #     '../src/libtracker-sparql/tracker-generated-no-checks.h', needed by
    #     'src/libtracker-sparql/tracker-generated.h', missing and no known
    #     rule to make it
    #
    output: 'tracker-generated.h',
    command: [join_paths(meson.current_source_dir(), 'tracker-sparql-add-include-guards.sh'), join_paths(meson.current_build_dir(), 'tracker-generated-no-checks.h'), '@OUTPUT@'],
    depends: libtracker_sparql_intermediate)

tracker_sparql_intermediate_dep = declare_dependency(
    link_with: [libtracker_sparql_intermediate],
    sources: tracker_sparql_generated_header,
    include_directories: [srcinc, include_directories('.')],
    dependencies: tracker_sparql_intermediate_dependencies
)

configure_file(
    input: 'tracker-sparql.pc.in',
    output: 'tracker-sparql-1.0.pc',
    configuration: conf,
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'pkgconfig'))

install_headers(
    'tracker-notifier.h',
    'tracker-resource.h',
    'tracker-ontologies.h',
    'tracker-sparql.h',
    'tracker-namespace-manager.h',
    'tracker-version.h',
    subdir: 'tracker-1.0/libtracker-sparql')

# FIXME: we need a custom script to install generated Vala headers; see
# <https://github.com/mesonbuild/meson/issues/705>.
meson.add_install_script('../install-generated-header.sh',
    join_paths(meson.current_build_dir(), 'tracker-generated.h'),
    join_paths(get_option('prefix'), get_option('includedir'), 'tracker-1.0', 'libtracker-sparql'))

custom_target('tracker-sparql-typelib',
    command: ['g-ir-compiler', '--output', '@OUTPUT@', join_paths(meson.current_build_dir(), 'Tracker-1.0.gir')],
    #input: join_paths(meson.current_build_dir(), 'Tracker-1.0.gir'),
    output: 'Tracker-1.0.typelib',
    depends: libtracker_sparql_intermediate,
    install: true,
    install_dir: join_paths(get_option('libdir'), 'girepository-1.0'))

# FIXME: reusing the same script to install the .gir; see
# <https://github.com/mesonbuild/meson/issues/891>.
meson.add_install_script('../install-generated-header.sh',
    join_paths(meson.current_build_dir(), 'Tracker-1.0.gir'),
    join_paths(get_option('prefix'), get_option('datadir'), 'gir-1.0'))

# FIXME: we can't depend properly on the generated .vapi to do this.
custom_target('tracker-sparql-' + tracker_api_version + '.vapi',
    #input: 'tracker-sparql-vala.vapi',
    output: 'tracker-sparql-' + tracker_api_version + '.vapi',
    command: ['sed', '-e', 's/tracker-generated-no-checks.h/tracker-sparql.h/', join_paths(meson.current_build_dir(), 'tracker-sparql.vapi')],
    depends: libtracker_sparql_intermediate,
    capture: true,
    install: true,
    install_dir: vapi_dir)

configure_file(
    input: 'tracker-sparql.deps',
    output: 'tracker-sparql-' + tracker_api_version + '.deps',
    configuration: configuration_data(), # No changes here, just a rename
    install: true,
    install_dir: vapi_dir)

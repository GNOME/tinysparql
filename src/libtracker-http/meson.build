module_sources = files('tracker-http-module.c')

http_common_deps = [avahi_glib, avahi_client]

tsparql_http_lib = static_library('libtracker-http',
  'tracker-http.c',
  dependencies: [gobject, gmodule],
  c_args: [
    '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
    '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
    # Global build root is required to detect noinst situations (tests/introspection),
    # event when built as a subproject
    '-DBUILDROOT="@0@"'.format(meson.global_build_root()),
  ] + tracker_c_args,
  include_directories: [configinc, srcinc],
)


libtracker_http_soup3 = shared_module('tracker-http-soup3',
    module_sources,
    dependencies: [libsoup3, http_common_deps],
    c_args: tracker_c_args + [
        '-include', join_paths(build_root, 'config.h'),
        '-include', '../libtracker-sparql/tracker-enums-private.h',
        '-DMODULE',
    ],
    install: true,
    install_dir: tracker_internal_libs_dir,
    name_suffix: 'so',
)
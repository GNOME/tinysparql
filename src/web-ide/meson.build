web_ide_static_files = files(
    'dist/main.js',
    'dist/index.html'
)

web_ide_gresources = gnome.compile_resources(
    'web_ide_gresources', 
    'web-ide.gresource.xml',
    dependencies: web_ide_static_files
)

web_ide_files = files(
    'tracker-webide.c'
)

web_ide_lib = static_library('tracker-webide',
    web_ide_files,
    c_args: [
        '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
        '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
        # Global build root is required to detect noinst situations (tests/introspection),
        # event when built as a subproject
        '-DBUILDROOT="@0@"'.format(meson.global_build_root()),
    ] + tracker_c_args,

    dependencies: [tsparql_http_dep, gobject, gmodule],
    include_directories: [configinc, httpinc, srcinc],
)

web_ide_dep = declare_dependency(
    link_with: web_ide_lib,
    include_directories: [srcinc, include_directories('.')],
    dependencies: [glib, gio],
)
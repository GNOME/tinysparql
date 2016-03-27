tracker_testcommon_dependencies = [glib]

libtracker_testcommon = library('tracker-testcommon',
    'tracker-test-helpers.c',
    dependencies: tracker_testcommon_dependencies,
    c_args: [
        '-DTEST_TEXT="@0@/tests/libtracker-common/non-utf8.txt"'.format(meson.source_root()),
    ]
)

tracker_testcommon_dep = declare_dependency(
    link_with: libtracker_testcommon,
    dependencies: tracker_testcommon_dependencies,
    include_directories: include_directories('.'),
)

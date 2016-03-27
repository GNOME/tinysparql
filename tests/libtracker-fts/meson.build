test_c_args = [
  '-DTRACKER_COMPILATION',
  '-DTOP_BUILDDIR="@0@/"'.format(meson.build_root()),
  '-DTOP_SRCDIR="@0@/"'.format(meson.source_root()),
]

fts_test = executable('tracker-fts-test',
  'tracker-fts-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep, tracker_data_dep, tracker_testcommon_dep],
  c_args: test_c_args
)

test('fts', fts_test)

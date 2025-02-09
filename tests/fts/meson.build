fts_test = executable('tracker-fts-test',
  'tracker-fts-test.c',
  dependencies: [tracker_sparql_dep],
  c_args: test_c_args
)

tests += {
  'name': 'fts',
  'exe': fts_test,
  'suite': ['fts'],
}

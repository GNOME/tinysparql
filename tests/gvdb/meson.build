gvdb_test = executable('gvdb-test',
    'gvdb-test.c',
    dependencies: [gio, gvdb_dep],
    include_directories: configinc,
)

tests += {
  'name': 'gvdb',
  'exe': gvdb_test,
  'suite': ['gvdb']
}

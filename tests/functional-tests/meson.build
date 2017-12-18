subdir('ipc')

test_runner = find_program('test-runner.sh')

functional_tests = [
  '01-insertion',
  '02-sparql-bugs',
  '03-fts-functions',
  '04-group-concat',
  '05-coalesce',
  '06-distance',
  '07-graph',
  '08-unique-insertions',
  '09-concurrent-query',
  '10-sqlite-misused',
  '11-sqlite-batch-misused',
  '12-transactions',
  '13-threaded-store',
  '14-signals',
  '15-statistics',
  '16-collation',
  '17-ontology-changes',
  '200-backup-restore',
]

foreach t: functional_tests
  test('functional-' + t, test_runner,
    args: './' + t + '.py',
    workdir: meson.current_source_dir(),
    # FIXME: these tests are all too slow
    timeout: 180)
endforeach

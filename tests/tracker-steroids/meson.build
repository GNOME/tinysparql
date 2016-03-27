test_c_args = ['-DTRACKER_COMPILATION']

steroids_test = executable('tracker-steroids-test',
  'tracker-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: test_c_args)
test('steroids', steroids_test)

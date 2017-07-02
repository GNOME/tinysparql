test_c_args = [
  '-DTEST_DOMAIN_ONTOLOGY_RULE="@0@/src/tracker-store/default.rule"'.format(meson.current_source_dir()),
  '-DTEST_ONTOLOGIES_DIR="@0@/src/ontologies/nepomuk"'.format(meson.current_source_dir()),
]

steroids_test = executable('tracker-steroids-test',
  'tracker-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: [tracker_c_args, test_c_args])
test('steroids', steroids_test)

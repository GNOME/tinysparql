functional_ipc_test_c_args = [
  '-DTEST_DOMAIN_ONTOLOGY_RULE="@0@"'.format(tracker_uninstalled_domain_rule),
  '-DTEST_ONTOLOGIES_DIR="@0@"'.format(tracker_uninstalled_nepomuk_ontologies_dir),
]

insert_or_replace_test = executable('test-insert-or-replace',
  'test-insert-or-replace.vala', tracker_sparql_vapi,
  dependencies: [tracker_common_dep, tracker_sparql_dep])
test('functional-ipc-insert-or-replace', insert_or_replace_test,
  env: test_env)

bus_query_cancellation_test = executable('test-bus-query-cancellation',
  'test-bus-query-cancellation.c',
  c_args: functional_ipc_test_c_args,
  dependencies: [tracker_common_dep, tracker_sparql_dep])

test('functional-ipc-bus-query-cancellation',
  test_runner,
  args: bus_query_cancellation_test,
  env: test_env)

bus_query_cancellation_test = executable('test-bus-query-cancellation',
  'test-bus-query-cancellation.c',
  c_args: test_c_args,
  dependencies: [tracker_common_dep, tracker_sparql_dep])

test('bus-query-cancellation', bus_query_cancellation_test,
  env: test_env,
  suite: ['functional', 'ipc'],
  timeout: 30)

libtracker_sparql_test_c_args = tracker_c_args + [
  '-DTEST',
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
]

libtracker_sparql_test_deps = [
    tracker_common_dep, tracker_sparql_dep
]

tracker_resource_test = executable('tracker-resource-test',
  'tracker-resource-test.c',
  dependencies: libtracker_sparql_test_deps,
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'resource',
  'exe': tracker_resource_test,
  'suite': ['resource'],
}

tracker_batch_test = executable('tracker-batch-test',
  'tracker-batch-test.c',
  dependencies: libtracker_sparql_test_deps,
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'batch',
  'exe': tracker_batch_test,
  'suite': ['sparql'],
}

tracker_connection_test = executable('tracker-connection-test',
  'tracker-connection-test.c',
  dependencies: libtracker_sparql_test_deps,
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'connection',
  'exe': tracker_connection_test,
  'suite': ['sparql']
}

tracker_sparql_test = executable('tracker-sparql-test',
  'tracker-sparql-test.c',
  dependencies: libtracker_sparql_test_deps,
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'sparql',
  'exe': tracker_sparql_test,
  'suite': ['sparql'],
  'is_parallel': false,
}

tracker_fd_test = executable('tracker-fd-test',
  'tracker-fd-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'fd',
  'exe': tracker_fd_test,
  'suite': ['sparql'],
}

tracker_cursor_test = executable('tracker-cursor-test',
  'tracker-cursor-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args)

tests += {
  'name': 'cursor+json',
  'exe': tracker_cursor_test,
  'suite': ['sparql'],
  'env': { 'TRACKER_TEST_PREFERRED_CURSOR_FORMAT': '0' },
}

tests += {
  'name': 'cursor+xml',
  'exe': tracker_cursor_test,
  'suite': ['sparql'],
  'env': { 'TRACKER_TEST_PREFERRED_CURSOR_FORMAT': '1' },
}

test_gresources = gnome.compile_resources('test_gresources', 'statement-queries.gresource.xml')

tracker_statement_test = executable('tracker-statement-test',
  'tracker-statement-test.c',
  test_gresources,
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args + test_c_args)

tests += {
  'name': 'statement',
  'exe': tracker_statement_test,
  'suite': ['sparql'],
}

tracker_serialize_test = executable('tracker-serialize-test',
  'tracker-serialize-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args + test_c_args)

tests += {
  'name': 'serialize',
  'exe': tracker_serialize_test,
  'suite': ['sparql'],
}

tracker_deserialize_test = executable('tracker-deserialize-test',
  'tracker-deserialize-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args + test_c_args)

tests += {
  'name': 'deserialize',
  'exe': tracker_deserialize_test,
  'suite': ['sparql'],
}

tracker_namespaces_test = executable('tracker-namespaces-test',
  'tracker-namespaces-test.c',
  dependencies: [tracker_common_dep, tracker_sparql_dep],
  c_args: libtracker_sparql_test_c_args + test_c_args)

tests += {
  'name': 'namespaces',
  'exe': tracker_namespaces_test,
  'suite': ['sparql'],
}

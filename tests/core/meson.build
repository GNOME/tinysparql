libtracker_data_tests = [
    'initialization',
    'ontology-change',
    'ontology-error',
    'refcount',
    'sparql-blank',
    'insert-or-replace',
    'service',
]

libtracker_data_slow_tests = [
    'ontology',
    'sparql'
]

libtracker_data_test_deps = [tracker_common_dep, tracker_sparql_dep]

foreach base_name: libtracker_data_tests
    source = 'tracker-@0@-test.c'.format(base_name)
    binary_name = 'tracker-@0@-test'.format(base_name)

    binary = executable(binary_name, source,
      dependencies: libtracker_data_test_deps,
      c_args: test_c_args)

    tests += {
      'name': base_name,
      'exe': binary,
      'suite': ['core'],
    }
endforeach

foreach base_name: libtracker_data_slow_tests
    source = 'tracker-@0@-test.c'.format(base_name)
    binary_name = 'tracker-@0@-test'.format(base_name)

    binary = executable(binary_name, source,
      dependencies: libtracker_data_test_deps,
      c_args: test_c_args)

      tests += {
        'name': base_name,
        'exe': binary,
        'suite': ['core', 'slow'],
        'timeout': 100
    }
endforeach

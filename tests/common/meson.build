libtracker_common_tests = [
    'file-utils',
    'parser',
    'utils',
]

libtracker_common_test_deps = [
    tracker_common_dep,
]

foreach base_name: libtracker_common_tests
    source = 'tracker-@0@-test.c'.format(base_name)
    binary_name = 'tracker-@0@-test'.format(base_name)

    binary = executable(binary_name, source,
      dependencies: libtracker_common_test_deps,
      c_args: test_c_args)

    tests += {
        'name': base_name,
        'exe': binary,
        'suite': ['common']
    }
endforeach

# This is a manual test case
parser_testcase = executable('tracker-parser',
    'tracker-parser.c',
    dependencies: tracker_common_dep,
    c_args: test_c_args,
)

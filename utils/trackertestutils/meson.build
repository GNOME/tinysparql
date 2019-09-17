pkg = import('pkgconfig')

sources = [
  '__init__.py',
  'dbusdaemon.py',
  'dconf.py',
  'helpers.py',
  'mainloop.py',
  'psutil_mini.py',
]

install_test_utils = false
if get_option('test_utils') == 'yes' or get_option('test_utils') == 'auto'
  arch_independent_libdir = \
    get_option('prefix') / 'lib' / 'tracker-' + tracker_api_version
  testutils_dir = join_paths(arch_independent_libdir, 'trackertestutils')
  install_test_utils = true
elif get_option('test_utils') != 'no' and get_option('test_utils') != ''
  testutils_dir = get_option('test_utils')
  install_test_utils = true
endif

if install_test_utils
  install_data(sources, install_dir: testutils_dir)

  pkg.generate(
    name: 'tracker-testutils-' + tracker_api_version,
    description: 'tracker test utilities',
    variables: [
      'python_path=' + testutils_dir
    ]
  )
endif

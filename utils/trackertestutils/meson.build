sources = [
  '__init__.py',
  '__main__.py',
  'dbusdaemon.py',
  'dconf.py',
  'helpers.py',
  'mainloop.py',
  'psutil_mini.py',
  'sandbox.py',
  'storehelper.py',
]

if get_option('test_utils')
  testutils_dir = tracker_internal_libs_dir / get_option('test_utils_dir')

  install_data(sources, install_dir: testutils_dir / 'trackertestutils')

  script_conf = configuration_data()
  script_conf.set('typelibdir', typelib_dir)
  script_conf.set('prefix', get_option('prefix'))
  script_conf.set('testutils_dir', testutils_dir)
  configure_file(
    input: 'tracker-sandbox.in',
    output: 'tracker-sandbox',
    configuration: script_conf,
    install_dir: testutils_dir / 'trackertestutils')

  pkg.generate(
    name: 'tracker-testutils-' + tracker_api_version,
    description: 'tracker test utilities',
    variables: [
      'python_path=' + ( '${libdir}/tracker-' + tracker_api_version ) / get_option('test_utils_dir'),
      'command=${python_path}/trackertestutils/tracker-sandbox',
    ]
  )

  subdir('await_file')

  configure_file(
    input: 'tracker-await-file.in',
    output: 'tracker-await-file',
    configuration: script_conf,
    install_dir: testutils_dir / 'trackertestutils')
endif

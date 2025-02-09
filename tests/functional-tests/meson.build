if get_option('tests_tap_protocol')
  protocol = 'tap'
else
  protocol = 'exitcode'
endif

testconf = configuration_data()

config_json_full_path = join_paths(meson.current_build_dir(), 'config', 'configuration.json')

testconf.set('TEST_CLI_DIR', tracker_uninstalled_cli_dir)
testconf.set('TEST_DBUS_DAEMON_CONFIG_FILE', build_root / 'tests' / 'test-bus.conf')
testconf.set('TEST_PORTAL_FLATPAK_INFO', source_root / 'tests' / 'flatpak-info')
testconf.set('TRACKER_VERSION', meson.project_version())
testconf.set('TEST_TAP_ENABLED', get_option('tests_tap_protocol').to_string())

subdir('config')

functional_tests = [
  'test_cli',
  'test_coalesce',
  'test_collation',
  'test_concurrent_query',
  'test_distance',
  'test_endpoint_http',
  'test_fts_functions',
  'test_graph',
  'test_group_concat',
  'test_insertion',
  'test_notifier',
  'test_ontology_changes',
  'test_ontology_rollback',
  'test_portal',
  'test_query',
  'test_sparql_bugs',
]

test_env = environment()
test_env.prepend('GI_TYPELIB_PATH', tracker_sparql_uninstalled_dir)
test_env.prepend('LD_LIBRARY_PATH', tracker_sparql_uninstalled_dir)
test_env.prepend('PYTHONPATH', tracker_uninstalled_testutils_dir)
test_env.set('TRACKER_FUNCTIONAL_TEST_CONFIG', config_json_full_path)
test_env.set('GIO_USE_VFS', 'local')
test_env.set('LANG', 'C')
test_env.append('TRACKER_DEBUG', 'fts-integrity', separator: ' ')
test_env.prepend('MANPATH', uninstalled_manpath)

foreach test_name: functional_tests
  file = meson.current_source_dir() / '@0@.py'.format(test_name)
  test(test_name, python,
    args: [file],
    env: test_env,
    protocol: protocol,
    suite: ['functional'],
    timeout: 30)
endforeach

subdir('test_ipc')

env = environment()
env.prepend('GI_TYPELIB_PATH', tracker_sparql_uninstalled_dir)
env.prepend('LD_LIBRARY_PATH', tracker_sparql_uninstalled_dir)
env.prepend('PYTHONPATH', tracker_uninstalled_testutils_dir)
env.set('TRACKER_EXAMPLES_AUTOMATED_TEST', '1')
env.set('TEST_ONTOLOGIES_DIR', tracker_uninstalled_nepomuk_ontologies_dir)
env.set('TRACKER_LANGUAGE_STOP_WORDS_DIR', join_paths(source_root, 'src', 'libtracker-common', 'stop-words'))

sandbox_python_args = ['-m', 'trackertestutils',
                       '--store-tmpdir',
                       '--dbus-config', meson.current_build_dir() / '..' / '..' / 'tests' / 'test-bus.conf']

examples = [
  'endpoint',
]

foreach example_name: examples
  file = meson.current_source_dir() / '@0@.py'.format(example_name)
  test(example_name, python,
    args: sandbox_python_args + [python.path(), file],
    env: env,
    suite: 'examples')
endforeach

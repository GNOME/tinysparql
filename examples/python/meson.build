env = environment()
env.prepend('GI_TYPELIB_PATH', tracker_sparql_uninstalled_dir)
env.prepend('LD_LIBRARY_PATH', tracker_sparql_uninstalled_dir)
env.prepend('PYTHONPATH', tracker_uninstalled_testutils_dir)
env.set('TRACKER_EXAMPLES_AUTOMATED_TEST', '1')

examples = [
  'endpoint',
]

foreach example_name: examples
  file = meson.current_source_dir() / '@0@.py'.format(example_name)
  test(example_name, python,
    args: file,
    env: env,
    suite: 'examples')
endforeach

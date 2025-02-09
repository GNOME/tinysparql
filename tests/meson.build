tests = []

test_bus_conf_file = configure_file(
  input: 'test-bus.conf.in',
  output: 'test-bus.conf',
  configuration: conf)

subdir('fts')
subdir('core')
subdir('common')
subdir('libtinysparql')
subdir('functional-tests')
subdir('services')

if get_option('tests_tap_protocol')
  protocol = 'tap'
else
  protocol = 'exitcode'
endif

foreach t: tests
  test_name = t.get('name')
  test_exe = t.get('exe')
  test_suite = t.get('suite', [])
  test_timeout = t.get('timeout', 30)
  test_env = t.get('env', {})

  # This flag marks unit tests which can't run alongside other unit tests. It
  # generally indicates a bug or design flaw in that test.
  test_is_parallel = t.get('is_parallel', true)

  env = environment()
  env.set('LANG', 'en_US.UTF8')
  env.set('LANGUAGE', 'en_US:C:POSIX')
  env.append('TINYSPARQL_DEBUG', 'fts-integrity', separator: ' ')

  foreach envvar_name, envvar_val: test_env
    env.set(envvar_name, envvar_val)
  endforeach

  test(test_name, test_exe,
    env: env,
    timeout: test_timeout,
    protocol: protocol,
    suite: test_suite,
    is_parallel: test_is_parallel)
endforeach

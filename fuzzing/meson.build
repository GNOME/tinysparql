fuzz_targets = [
    'fuzz_query',
    'fuzz_rdf_jsonld',
    'fuzz_rdf_trig',
    'fuzz_rdf_turtle',
]

deps = [gio, glib, tracker_sparql_dep, sqlite]
extra_sources = []
extra_c_args = cc.get_supported_arguments('-Werror=unused-function')

# Links in a static library provided by oss-fuzz, else a standalone driver.
# https://google.github.io/oss-fuzz/getting-started/new-project-guide/#buildsh-script-environment
have_fuzzing_engine = false
if have_cxx
    cxx = meson.get_compiler('cpp')
    fuzzing_engine = cxx.find_library('FuzzingEngine', required : get_option('oss_fuzz'))
    have_fuzzing_engine = fuzzing_engine.found()
endif
if have_fuzzing_engine
    deps += fuzzing_engine
else
    extra_sources += 'driver.c'
endif

link_args = []
if host_machine.system() == 'linux'
    link_args = ['-Wl,--disable-new-dtags']
endif

foreach target_name : fuzz_targets
    exe = executable(target_name, [extra_sources, target_name + '.c'],
        c_args : extra_c_args,
        dependencies : deps,
        # Prefer RPATH over RUNPATH so the executable
        # always finds the uninstalled library.
        link_args: link_args,
    )

    samples_dir = meson.current_source_dir() / 'samples' / target_name.replace('fuzz_', '')
    samples = run_command('get-samples.sh', samples_dir, check: true).stdout().strip().split('\n')

    # If the FuzzingEngine isn’t available, build some unit tests to check that
    # the fuzzing files do basically work. This doesn’t do any actual fuzzing though.
    # Pass in the README as an arbitrary fuzzing input, just so we have something.
    if get_option('tests') and not have_fuzzing_engine
        test(target_name, exe,
            args : [samples, files('README.md')],
            suite : 'fuzzing',
        )
    endif
endforeach

tracker_common_sources = [
  'tracker-date-time.c',
  'tracker-debug.c',
  'tracker-file-utils.c',
  'tracker-term-utils.c',
  'tracker-utils.c',
]

parser_deps = []

if get_option('builtin_modules')
  libtracker_parser = []
  tracker_common_sources += [
    'tracker-language.c',
    'tracker-parser-utils.c'
  ]

  if unicode_library_name == 'icu'
    parser_deps = [libstemmer, icu_uc, icu_i18n]
    tracker_common_sources += 'tracker-parser-libicu.c'
  else
    parser_deps = [libstemmer, libunistring]
    tracker_common_sources += 'tracker-parser-libunistring.c'
  endif
else
  tracker_common_sources += 'tracker-parser.c'

  if unicode_library_name == 'icu'
    libtracker_parser = shared_module('tracker-parser-libicu',
      'tracker-parser-libicu.c',
      'tracker-parser-utils.c',
      'tracker-language.c',
      dependencies: [gio, libstemmer, icu_uc, icu_i18n],
      c_args: tracker_c_args + [
        '-include', join_paths(build_root, 'config.h'),
        '-DMODULE',
      ],
      include_directories: [configinc, srcinc],
      install: true,
      install_dir: tracker_internal_libs_dir,
      name_suffix: 'so',
    )
  else
    libtracker_parser = shared_module('tracker-parser-libunistring',
      'tracker-parser-libunistring.c',
      'tracker-parser-utils.c',
      'tracker-language.c',
      dependencies: [gio, libstemmer, libunistring],
      c_args: tracker_c_args + [
        '-include', join_paths(build_root, 'config.h'),
        '-DMODULE',
      ],
      include_directories: [configinc, srcinc],
      install: true,
      install_dir: tracker_internal_libs_dir,
      name_suffix: 'so',
    )
  endif
endif

tracker_common_dependencies = [glib, gio, gio_unix, libmath, parser_deps]

if build_machine.system() == 'openbsd'
  libkvm = meson.get_compiler('c').find_library('kvm')
  tracker_common_dependencies += libkvm
endif

libtracker_common = static_library('tracker-common',
  tracker_common_sources,
  dependencies: [tracker_common_dependencies, gmodule],
  c_args: [
    '-DPRIVATE_LIBDIR="@0@"'.format(tracker_internal_libs_dir),
    '-DBUILD_LIBDIR="@0@"'.format(meson.current_build_dir()),
    # Global build root is required to detect noinst situations (tests/introspection),
    # event when built as a subproject
    '-DBUILDROOT="@0@"'.format(meson.global_build_root()),
  ] + tracker_c_args,
  include_directories: [configinc, srcinc],
  gnu_symbol_visibility: 'hidden',
)

commoninc = include_directories('.')

tracker_common_dep = declare_dependency(
  link_with: libtracker_common,
  dependencies: [tracker_common_dependencies, gmodule],
  include_directories: [configinc, srcinc, commoninc],
)

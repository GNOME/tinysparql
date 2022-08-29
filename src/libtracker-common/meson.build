subdir('stop-words')

tracker_common_sources = [
  'tracker-date-time.c',
  'tracker-debug.c',
  'tracker-file-utils.c',
  'tracker-term-utils.c',
  'tracker-utils.c',
  'tracker-locale.c',
  'tracker-parser-utils.c',
  'tracker-language.c',
]

if unicode_library_name == 'icu'
  tracker_common_sources += 'tracker-parser-libicu.c'
else
  tracker_common_sources += 'tracker-parser-libunistring.c'
endif

tracker_common_dependencies = [glib, gio, gio_unix, libmath, libstemmer]

if build_machine.system() == 'openbsd'
  libkvm = meson.get_compiler('c').find_library('kvm')
  tracker_common_dependencies += libkvm
endif

libtracker_common = static_library('tracker-common',
  tracker_common_sources,
  dependencies: tracker_common_dependencies + [unicode_library],
  c_args: tracker_c_args,
  include_directories: [configinc, srcinc],
  gnu_symbol_visibility: 'hidden',
)

commoninc = include_directories('.')

tracker_common_dep = declare_dependency(
  link_with: libtracker_common,
  dependencies: tracker_common_dependencies + [unicode_library],
  include_directories: [configinc, srcinc, commoninc],
)

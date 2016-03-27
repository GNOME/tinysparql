libtracker_extract_sources = [
  'tracker-encoding.c',
  'tracker-exif.c',
  'tracker-extract-info.c',
  'tracker-guarantee.c',
  'tracker-iptc.c',
  'tracker-module-manager.c',
  'tracker-resource-helpers.c',
  'tracker-utils.c',
  'tracker-xmp.c',
]

if charset_library_name == 'enca'
  libtracker_extract_sources += 'tracker-encoding-enca.c'
elif charset_library_name == 'icu'
  libtracker_extract_sources += 'tracker-encoding-libicu.c'
endif

if have_meegotouch
 libtracker_extract_sources += 'tracker-encoding-meegotouch.cpp'
endif

tracker_extract_dependencies = [charset_library, gmodule]

if exempi.found()
  tracker_extract_dependencies += exempi
endif

if libexif.found()
  tracker_extract_dependencies += libexif
endif

if libiptcdata.found()
  tracker_extract_dependencies += libiptcdata
endif

libtracker_extract = library('tracker-extract',
  libtracker_extract_sources,
  dependencies: [tracker_common_dep, tracker_sparql_dep] + tracker_extract_dependencies,
  c_args: [
    '-DTRACKER_COMPILATION',
    '-DTRACKER_EXTRACTOR_RULES_DIR="@0@"'.format(tracker_extract_rules_dir),
    '-DTRACKER_EXTRACTORS_DIR="@0@"'.format(tracker_extract_modules_dir)
  ],
  install: true,
  install_rpath: tracker_internal_libs_dir,
)

tracker_extract_dep = declare_dependency(
  link_with: libtracker_extract,
  dependencies: [tracker_sparql_dep] + tracker_extract_dependencies,
  include_directories: srcinc)

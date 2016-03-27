test_c_args = [
  '-DTRACKER_COMPILATION',
  '-DTOP_BUILDDIR="@0@/"'.format(meson.build_root()),
  '-DTOP_SRCDIR="@0@/"'.format(meson.source_root()),
]

guarantee_test = executable('tracker-guarantee-test',
  'tracker-guarantee-test.c',
  dependencies: [tracker_common_dep, tracker_extract_dep],
  c_args: test_c_args,
)
test('extract-guarantee', guarantee_test)

extract_info_test = executable('tracker-extract-info-test',
  'tracker-extract-info-test.c',
  dependencies: [tracker_common_dep, tracker_extract_dep],
  c_args: test_c_args,
)
test('extract-info-test', extract_info_test)

utils_test = executable('tracker-test-utils',
  'tracker-test-utils.c',
  dependencies: [tracker_common_dep, tracker_extract_dep],
  c_args: test_c_args,
)
test('extract-utils', utils_test)

xmp_test = executable('tracker-test-xmp',
  'tracker-test-xmp.c',
  dependencies: [tracker_common_dep, tracker_extract_dep],
  c_args: test_c_args,
)
test('extract-xmp', xmp_test)

if libexif.found()
  exif_test = executable('tracker-exif-test',
    'tracker-exif-test.c',
    dependencies: [tracker_common_dep, tracker_extract_dep],
    c_args: test_c_args,
  )
  test('extract-exif', exif_test)
endif

if libiptcdata.found() and libjpeg.found()
  iptc_test = executable('tracker-iptc-test',
    'tracker-iptc-test.c',
    dependencies: [tracker_common_dep, tracker_extract_dep, libjpeg],
    c_args: test_c_args,
  )
  test('extract-iptc', iptc_test)
endif

if charset_library_name != 'none'
  encoding_test = executable('tracker-encoding-test',
    'tracker-encoding-test.c',
    dependencies: [tracker_common_dep, tracker_extract_dep],
    c_args: test_c_args,
  )
  test('extract-encoding', encoding_test)
endif

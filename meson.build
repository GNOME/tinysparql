project('tracker', 'c', 'vala', 'cpp', version: '1.99.3')

gnome = import('gnome')
i18n = import('i18n')
cc = meson.get_compiler('c')

# This is the X.Y used in -llibtracker-FOO-X.Y
tracker_api_version = '2.0'

glib_required = '2.40.0'

# 3.6.11 for sqlite_backup API
# 3.6.16 to fix test failures
# 3.6.17 for shared cache mode with virtual tables
# 3.7.0 for WAL
# 3.7.9 for FTS4 content= support
sqlite_required = '3.7.9'

avcodec = dependency('libavcodec', version: '>= 0.8.4', required: false)
avformat = dependency('libavformat', version: '>= 0.8.4', required: false)
avutil = dependency('libavutil', version: '>= 0.8.4', required: false)
cairo = dependency('cairo', version: '> 1.0')
camel = dependency('camel-1.2', version: '> 2.32.0', required: false)
dbus = dependency('dbus-1', version: '> 1.3.1')
exempi = dependency('exempi-2.0', version: '> 2.1.0', required: false)
flac = dependency('flac', version: '> 1.2.1', required: false)
gio = dependency('gio-2.0', version: '>' + glib_required)
gio_unix = dependency('gio-unix-2.0', version: '>' + glib_required)
glib = dependency('glib-2.0', version: '>' + glib_required)
gmodule = dependency('gmodule-2.0', version: '>' + glib_required)
gobject = dependency('gobject-2.0', version: '>' + glib_required)
gobject_introspection = dependency('gobject-introspection-1.0')
gstreamer = dependency('gstreamer-1.0', required: false)
gstreamer_pbutils = dependency('gstreamer-pbutils-1.0', required: false)
gstreamer_tag = dependency('gstreamer-tag-1.0', required: false)
gtk3 = dependency('gtk+-3.0', version: '> 3.0.0', required: false)
gupnp_dlna = dependency('gupnp-dlna-2.0', version: '> 0.9.4', required: false)
gupnp_dlna_gst = dependency('gupnp-dlna-gst-2.0', version: '> 0.9.4', required: false)
icu_i18n = dependency('icu-i18n', version: '> 4.8.1.1', required: false)
icu_uc = dependency('icu-uc', version: '> 4.8.1.1', required: false)
json_glib = dependency('json-glib-1.0', version: '>= 1.0', required: true)
libcue = dependency('libcue', required: false)
libemail_utils = dependency('libemail-utils', required: false)
libemail_engine = dependency('libemail-engine', required: false)
libexif = dependency('libexif', version: '> 0.6', required: false)
libgrss = dependency('libgrss', version: '> 0.7', required: false)
libgsf = dependency('libgsf-1', version: '> 1.14.24', required: false)
libgxps = dependency('libgxps', required: false)
libiptcdata = dependency('libiptcdata', required: false)
libosinfo = dependency('libosinfo', version: '> 0.2.9', required: false)
libpng = dependency('libpng', version: '> 0.89', required: false)
libsoup = dependency('libsoup-2.4', version: '> 2.40', required: true)
libtiff = dependency('libtiff-4', required: false)
libxml2 = dependency('libxml-2.0', version: '> 2.6')
libvorbis = dependency('vorbisfile', version: '> 0.22')
network_manager = dependency('libnm')
poppler = dependency('poppler-glib', version: '> 0.16.0', required: false)
sqlite = dependency('sqlite3', version: '>' + sqlite_required)
taglib = dependency('taglib_c', version: '> 1.6', required: false)
totem_plparser = dependency('totem-plparser', required: false)
upower = dependency('upower-glib', version: '> 0.9.0', required: false)
zlib = dependency('zlib')

libgif = cc.find_library('libgif', required: false)
libjpeg = cc.find_library('libjpeg', required: false)
libmath = cc.find_library('m')

# FIXME: here we work around a quirk todo with Meson and Vala: the 'uuid'
# pkg-config component corresponds to 'libuuid.vapi', but Meson assumes that
# .vapi name always matches pkg-config name, and will add `--pkg uuid` to the
# Vala commandline. By finding 'uuid' using find_library() instead, we get
# a dependency object back that won't cause any `--pkg` arguments to be added
# to the Vala commandline, but still works as expected for C code.
uuid = cc.find_library('uuid')

# There are various gchar vs. unsigned char warnings that occur in extract
# modules, it's not worth adding casts everywhere so we disable the warning.
add_project_arguments('-Wno-pointer-sign', language: 'c')
add_project_arguments('-DTRACKER_COMPILATION', language: 'c')

enable_fts = get_option('fts')
enable_extract = get_option('extract')
enable_writeback = get_option('writeback')

##################################################################
# Check for libtracker-common, make sure libstemmer exists
##################################################################

have_libstemmer = false
if get_option('stemmer') != 'no'
    stemmer = cc.find_library('stemmer')
    if stemmer.found()
        have_libstemmer = true
    elif get_option('stemmer') == 'yes'
        error('stemmer support explicitly requested, but stemmer library couldn\'t be found')
    endif
endif

##################################################################
# Check for tracker-fts, allow disabling FTS support
##################################################################

if enable_fts
  sqlite3_builtin_fts5_test = '''
    #include <sqlite3.h>
    sqlite3 *db;
    int rc;
    rc = sqlite3_open(":memory:", &db);
    if (rc!=SQLITE_OK) return -1;
    rc = sqlite3_exec(db, "create table a(text)", 0, 0, 0);
    if (rc!=SQLITE_OK) return -1;
    rc = sqlite3_exec(db, "create virtual table t using fts5(content='a',text)", 0, 0, 0);
    if (rc!=SQLITE_OK) return -1;
  '''

  sqlite3_has_builtin_fts5 = cc.compiles(sqlite3_builtin_fts5_test,
    name: 'sqlite3 has builtin FTS5 module')

  if not sqlite3_has_builtin_fts5 and sqlite.version() >= '3.20.0'
    error('sqlite3 >= 3.20.0 must be compiled with --enable-fts5')
  endif
endif

##################################################################
# Check for libtracker-common: battery/mains power detection
#
# By default, AUTO with this order of preference:
#  1)  upower
#  2)  hal
##################################################################

have_hal = false
battery_detection_library_name = ''

if upower.found()
  if get_option('battery_detection') == 'auto' or get_option('battery_detection') == 'upower'
    battery_detection_library = upower
    battery_detection_library_name = 'upower'
  endif
elif get_option('battery_detection') == 'upower'
  error('upower explicitly requested, but upower library was not found')
endif

if battery_detection_library_name == ''
  hal = dependency('hal', version: '> 0.5', required: false)

  if hal.found()
    battery_detection_library = hal
    battery_detection_library_name = 'hal'
  elif get_option('battery_detection') == 'hal'
    error('HAL explicitly requested, but not found')
  else
    battery_detection_library_name = '(none)'
  endif
endif

##################################################################
# Check for libtracker-data and libtracker-fts: Unicode support
#
# By default, AUTO with this order of preference:
#  1)  libunistring
#  2)  libicu
##################################################################

have_libicu = false
unicode_library_name = ''

if icu_i18n.found() and icu_uc.found()
  have_libicu = true
endif

if have_libicu
  if get_option('unicode_support') == 'auto' or get_option('unicode_support') == 'icu'
    unicode_library = declare_dependency(
        dependencies: [icu_uc, icu_i18n]
    )
    unicode_library_name = 'icu'
  endif
elif get_option('unicode_support') == 'icu'
  error('libicu explicitly requested, but icu-i18n and icu-uc libraries were not found')
endif

if unicode_library_name == ''
  libunistring = cc.find_library('libunistring', required: false)

  if libunistring.found()
    unicode_library = libunistring
    unicode_library_name = 'unistring'
  elif get_option('unicode_support') == 'unistring'
    error('libunistring explicitly requested, but not found')
  else
    error('Neither libicu or libunistring were found for Unicode support.')
  endif
endif

####################################################################
# This section is for tracker-extract dependencies
####################################################################

##########################################################################
# Check for tracker-extract: icu or enca for detecting Cyrillic in MP3s
#
# By default, AUTO with this order of preference:
#  1)  icu
#  2)  enca
##########################################################################

charset_library_name = 'none'

if have_libicu
  if get_option('charset_detection') == 'auto' or get_option('charset_detection') == 'icu'
    charset_library = declare_dependency(
       dependencies: [icu_uc, icu_i18n]
    )
    charset_library_name = 'icu'
  endif
else
  if get_option('charset_detection') == 'icu'
    error('libicu requested for character set detection, but it was not found')
  endif
endif

if charset_library_name == 'none' and get_option('charset_library') != 'none'
  enca = dependency('enca', version: '>= 1.9', required: false)

  if enca.found()
    charset_library = enca
    charset_library_name = 'enca'
  elif get_option('enca') == 'yes'
    error('enca support explicitly requested, but enca couldn\'t be found')
  endif
endif

####################################################################
# Check for tracker-extract: gstreamer/etc
####################################################################

generic_media_handler = []
generic_media_handler_name = 'none'

if get_option('generic_media_extractor') == 'auto'
  if gstreamer.found() and gstreamer_tag.found()
    generic_media_handler = [gstreamer, gstreamer_tag]
    generic_media_handler_name = 'gstreamer'
  elif avcodec.found() and avformat.found() and avutil.found()
    generic_media_handler = [avcodec, avformat, avutil]
    generic_media_handler_name = 'libav'
  else
    generic_media_handler_name = 'none'
    generic_media_handler = []
  endif
elif get_option('generic_media_extractor') == 'gstreamer'
  if gstreamer.found() and gstreamer_tag.found()
    generic_media_handler = [gstreamer, gstreamer_tag]
    generic_media_handler_name = 'gstreamer'
  else
    error('GStreamer media handler was enabled but required GStreamer libraries were not found')
  endif
elif get_option('generic_media_extractor') == 'libav'
  if avcodec.found() and avformat.found() and avutil.found()
    generic_media_handler = [avcodec, avformat, avutil]
    generic_media_handler_name = 'libav'
  else
    error('libav media handler was enabled but required libav libraries were not found')
  endif
endif

###########################################################################
# Check for gstreamer backend: tagreadbin/decodebin2/discoverer/gupnp-dlna
###########################################################################

gstreamer_backend_name = ''

if generic_media_handler_name == 'gstreamer'
  if get_option('gstreamer_backend') == 'discoverer'
    if gstreamer_pbutils.found()
      gstreamer_backend_name = 'Discoverer'
    else
      error('GStreamer Discoverer backend requires GStreamer pbutils library')
    endif
  elif get_option('gstreamer_backend') == 'gupnp'
    if gupnp_dlna.found() and gupnp_dlna_gst.found()
      gstreamer_backend_name = 'GUPnP-DNLA'
    else
      error('GStreamer GUPnP-DNLA backend requires the gupNp and gupnp-gst libraries')
    endif
  endif
endif

####################################################################
# D-Bus service files
####################################################################

if get_option('dbus_services') == ''
  dbus_services_dir = dbus.get_pkgconfig_variable('session_bus_services_dir')
else
  dbus_services_dir = get_option('dbus_services')
endif

####################################################################
# systemd user services
####################################################################

install_systemd_user_services = false
if get_option('systemd_user_services') == 'yes' or get_option('systemd_user_services') == 'auto'
  systemd = dependency('systemd', required: false)
  if systemd.found()
    systemd_user_services_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
  else
    systemd_user_services_dir = join_paths(get_option('libdir'), 'systemd', 'user')
  endif
  install_systemd_user_services = true
elif get_option('systemd_user_services') == 'no' or get_option('systemd_user_services') == ''
  install_systemd_user_services = false
  systemd_user_services_dir = '(disabled)'
else
  install_systemd_user_services = true
  systemd_user_services_dir = get_option('systemd_user_services')
endif

####################################################################
# bash-completion
####################################################################

install_bash_completion = false
if get_option('bash_completion') == 'yes' or get_option('bash_completion') == 'auto'
  bash_completion_package = dependency('bash-completion', required: false)
  if bash_completion_package.found()
    bash_completion_dir = bash_completion_package.get_pkgconfig_variable('completionsdir')
  else
    bash_completion_dir = join_paths(get_option('prefix'), get_option('datadir'), 'bash-completion', 'completions')
  endif
  install_bash_completion = true
elif get_option('bash_completion') == 'no' or get_option('bash_completion') == ''
  install_bash_completion = false
  bash_completion_dir = '(disabled)'
else
  install_bash_completion = true
  bash_completion_dir = get_option('bash_completion')
endif

tracker_major_version = meson.project_version().split('.')[0].to_int()
tracker_minor_version = meson.project_version().split('.')[1].to_int()
tracker_micro_version = meson.project_version().split('.')[2].to_int()

conf = configuration_data()

# Config that goes in config.h
conf.set('DISABLE_JOURNAL', get_option('journal') == false)
conf.set('USING_UNZIPPSFILES', get_option('unzip_ps_gz_files') == false)

conf.set('HAVE_TRACKER_EXTRACT', enable_extract)
conf.set10('HAVE_TRACKER_FTS', enable_fts)
conf.set('HAVE_TRACKER_WRITEBACK', enable_writeback)

conf.set('HAVE_BUILTIN_FTS', sqlite3_has_builtin_fts5)
conf.set('HAVE_ENCA', charset_library_name == 'enca')
conf.set('HAVE_EXEMPI', exempi.found())
conf.set('HAVE_GSTREAMER', generic_media_handler_name == 'gstreamer')
conf.set('GSTREAMER_BACKEND_DISCOVERER', gstreamer_backend_name == 'Discoverer')
conf.set('GSTREAMER_BACKEND_GUPNP_DLNA', gstreamer_backend_name == 'GUPnP-DNLA')
conf.set('HAVE_HAL', battery_detection_library_name == 'hal')
conf.set('HAVE_LIBCUE', libcue.found())
conf.set('HAVE_LIBCUE2', libcue.version() >= '2.0.0')
conf.set10('HAVE_LIBICU', unicode_library_name == 'icu')
conf.set('HAVE_LIBICU_CHARSET_DETECTION', charset_library_name == 'icu')
conf.set('HAVE_LIBEXIF', libexif.found())
conf.set('HAVE_LIBIPTCDATA', libiptcdata.found())
conf.set('HAVE_LIBSTEMMER', have_libstemmer)
conf.set('HAVE_LIBUNISTRING', unicode_library_name == 'unistring')
conf.set('HAVE_NETWORK_MANAGER', network_manager.found())
conf.set('HAVE_UPOWER', battery_detection_library_name == 'upower')

conf.set('HAVE_GETLINE', cc.has_function('getline', prefix : '#include <stdio.h>'))
conf.set('HAVE_POSIX_FADVISE', cc.has_function('posix_fadvise', prefix : '#include <fcntl.h>'))
conf.set('HAVE_STATVFS64', cc.has_header_symbol('sys/statvfs.h', 'statvfs64', args: '-D_LARGEFILE64_SOURCE'))
conf.set('HAVE_STRNLEN', cc.has_function('strnlen', prefix : '#include <string.h>'))

conf.set('LOCALEDIR', '"@0@/@1@"'.format(get_option('prefix'), get_option('localedir')))
conf.set('SHAREDIR', '"@0@/@1@"'.format(get_option('prefix'), get_option('datadir')))
conf.set('TRACKER_MINERS_DIR', '"@0@/@1@/tracker/miners"'.format(get_option('prefix'), get_option('datadir')))
conf.set('TRACKER_UI_DIR', '"@0@/@1@/tracker/"'.format(get_option('prefix'), get_option('datadir')))

conf.set('GETTEXT_PACKAGE', '"tracker"')
conf.set('PACKAGE_VERSION', '"@0@"'.format(meson.project_version()))
conf.set('TRACKER_VERSION', '"@0@"'.format(meson.project_version()))
conf.set('TRACKER_MAJOR_VERSION', tracker_major_version)
conf.set('TRACKER_MINOR_VERSION', tracker_minor_version)
conf.set('TRACKER_MICRO_VERSION', tracker_micro_version)
conf.set('TRACKER_INTERFACE_AGE', 0)
conf.set('TRACKER_BINARY_AGE', 100 * tracker_minor_version + tracker_micro_version)

# Config that goes in some other generated files (.desktop, .pc, etc)
conf.set('exec_prefix', get_option('prefix'))
conf.set('includedir', join_paths(get_option('prefix'), get_option('includedir')))
conf.set('libdir', join_paths(get_option('prefix'), get_option('libdir')))
conf.set('libexecdir', join_paths(get_option('prefix'), get_option('libexecdir')))
conf.set('prefix', get_option('prefix'))
conf.set('TRACKER_API_VERSION', '1.0')
conf.set('VERSION', meson.project_version())

configure_file(input: 'config.h.meson.in',
               output: 'config.h',
               configuration: conf)

# Global compiler defines. We avoid add_global_arguments() as it breaks
# when we are included by another project as a subproject.
tracker_c_args = [
  # Needed for O_NOATIME, and probably other stuff.
  '-D_GNU_SOURCE',

  # Needed for statvfs64, and probably other stuff
  '-D_LARGEFILE64_SOURCE',
]

have_tracker_miner_fs = true
have_tracker_miner_apps = true
have_tracker_miner_rss = true
have_tracker_extract = true
have_tracker_writeback = true
have_tracker_needle = true
have_tracker_preferences = true

configinc = include_directories('./')
srcinc = include_directories('src/')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()
po_subdir = join_paths(source_root, 'po')

tracker_internal_libs_dir = join_paths(get_option('prefix'), get_option('libdir'), 'tracker-' + tracker_api_version)

tracker_extract_modules_dir = join_paths(get_option('prefix'), get_option('libdir'), 'tracker-@0@/extract-modules'.format(tracker_api_version))
# NOTE: We don't use ${TRACKER_API_VERSION} because other content like
# the ontology is installed to the same location.
tracker_extract_rules_dir = join_paths(get_option('prefix'), get_option('datadir'), 'tracker/extract-rules')
tracker_miner_services_dir = join_paths(get_option('prefix'), get_option('datadir'), 'tracker/miners')
tracker_ontologies_dir = join_paths(get_option('prefix'), get_option('datadir'), 'tracker/ontologies')
tracker_writeback_modules_dir = join_paths(get_option('prefix'), get_option('libdir'), 'tracker-@0@/writeback-modules'.format(tracker_api_version))

vapi_dir = join_paths(get_option('prefix'), get_option('datadir'), 'vala', 'vapi')

g_ir_compiler = find_program('g-ir-compiler', gobject_introspection.get_pkgconfig_variable('g_ir_compiler'))
g_ir_merge = find_program('g-ir-merge', join_paths(meson.current_source_dir(), 'utils', 'g-ir-merge', 'g-ir-merge'))

# You can use the gobject_introspection.get_pkgconfig_variable() to find these,
# but then get_option('prefix') won't be honoured if it differs from the prefix
# gobject-introspection is installed to.
gir_dir = join_paths(get_option('prefix'), get_option('datadir'), 'gir-1.0')
typelib_dir = join_paths(get_option('libdir'), 'girepository-1.0')

subdir('src')

if get_option('docs')
  subdir('docs')
endif

subdir('examples')
subdir('utils')

subdir('tests')

subdir('po')

# Make sure we're not using versions of SQLite which cause problems.
#
# Avoid versions:
sqlite_safe = true
if sqlite.version().version_compare('>= 3.7.10') and sqlite.version().version_compare('<= 3.7.13')
  # See https://mail.gnome.org/archives/tracker-list/2012-October/msg00028.html
  sqlite_safe = false
elif sqlite.version().version_compare('3.8.1')
  # See https://mail.gnome.org/archives/tracker-list/2013-November/msg00021.html
  sqlite_safe = false
elif sqlite.version().version_compare('3.8.4.2')
  # See https://mail.gnome.org/archives/tracker-list/2014-April/msg00001.html
  sqlite_safe = false
endif

summary = [
  '\nBuild Configuration:',
  '    Prefix:                                 ' + get_option('prefix'),
  '    Source code location:                   ' + meson.current_source_dir(),
  '    Compiler:                               ' + cc.get_id(),
  '\nFeature Support:',
  '    Battery/mains power detection:          ' + battery_detection_library_name,
  '    Support for network status detection:   ' + network_manager.found().to_string(),
  '    Unicode support library:                ' + unicode_library_name,
  '    Build with Journal support:             ' + get_option('journal').to_string(),
  '    Build with SQLite FTS support:          @0@ (built-in FTS: @1@)'.format(
     get_option('fts'), sqlite3_has_builtin_fts5),
  '    Build with Stemming support:            ' + have_libstemmer.to_string(),
  '    Bash completion support:                ' + bash_completion_dir,
  '\nData Miners / Writebacks:',
  '    FS (File System):                       ' + have_tracker_miner_fs.to_string(),
  '    Applications:                           ' + have_tracker_miner_apps.to_string(),
  '    RSS:                                    ' + have_tracker_miner_rss.to_string(),
  '   Extract (secondary extraction):     ' + have_tracker_extract.to_string(),
  '   Writeback (writing changes back):   ' + have_tracker_writeback.to_string(),
]


if have_tracker_extract
  summary += [
    '\nMetadata Extractors:',
    '    Support PNG:                            ' + libpng.found().to_string(),
    '    Support PDF:                            ' + poppler.found().to_string(),
    '    Support XPS:                            ' + libgxps.found().to_string(),
    '    Support GIF:                            @0@ (xmp: @1@)'.format(libgif.found().to_string(), exempi.found().to_string()),
    '    Support JPEG:                           @0@ (xmp: @1@, exif: @2@, iptc: @3@)'.format(
        libjpeg.found().to_string(), exempi.found().to_string(), libexif.found().to_string(), libiptcdata.found().to_string()),
    '    Support TIFF:                           @0@ (xmp: @1@, exif: @2@, iptc: @3@)'.format(
        libtiff.found().to_string(), exempi.found().to_string(), libexif.found().to_string(), libiptcdata.found().to_string()),
    '    Support Vorbis (ogg/etc):               ' + libvorbis.found().to_string(),
    '    Support Flac:                           ' + flac.found().to_string(),
    '    Support MS & Open Office:               ' + libgsf.found().to_string(),
    '    Support XML / HTML:                     ' + libxml2.found().to_string(),
    '    Support embedded / sidecar XMP:         ' + exempi.found().to_string(),
    '    Support generic media formats:          @0@ (backend: @1@)'.format(
        generic_media_handler_name, gstreamer_backend_name),
    '    Support cue sheet parsing:              ' + libcue.found().to_string(),
    '    Support playlists (w/ Totem):           ' + totem_plparser.found().to_string(),
    '    Support ISO image parsing:              ' + libosinfo.found().to_string(),
    '    Support AbiWord document parsing:       true',
    '    Support DVI parsing:                    true',
    '    Support MP3 parsing:                    true',
    '    Support MP3 tag charset detection:      ' + charset_library_name,
    '    Support PS parsing:                     true',
    '    Support text parsing:                   true',
    '    Support icon parsing:                   true'
  ]
endif


if have_tracker_writeback
  summary += [
    '\nWriteback Formats:',
    '    Audio files using Taglib:               ' + taglib.found().to_string(),
    '    XMP:                                    ' + exempi.found().to_string(),
  ]
endif

summary += [
    '\nApplications:',
    '    Build tracker-preferences:              ' + have_tracker_preferences.to_string(),
    '    Build tracker-needle:                   ' + have_tracker_needle.to_string(),
    '\nFrameworks / Options:',
    '    Support Guaranteed Metadata             ' + get_option('guarantee_metadata').to_string(),
]

message('\n'.join(summary))

if not sqlite_safe
  message('''WARNING:
        SQLite3 version @0@ is not safe. For detail
        3.7.10 - 3.7.13: https://mail.gnome.org/archives/tracker-list/2012-October/msg00028.html
        3.8.1:           https://mail.gnome.org/archives/tracker-list/2013-November/msg00021.html
        3.8.4.2:         https://mail.gnome.org/archives/tracker-list/2014-April/msg00001.html
  ''').format(sqlite.version())
endif

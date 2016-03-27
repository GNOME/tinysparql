# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# This file is part of Tracker.

AC_PREREQ([2.64])

# Making releases:
#   TRACKER_MICRO_VERSION += 1;
#   TRACKER_INTERFACE_AGE += 1;
#   TRACKER_BINARY_AGE += 1;
# if any functions have been added, set TRACKER_INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set TRACKER_BINARY_AGE and TRACKER_INTERFACE_AGE to 0.
m4_define([tracker_major_version], [1])
m4_define([tracker_minor_version], [8])
m4_define([tracker_micro_version], [0])
m4_define([tracker_interface_age], [0])
m4_define([tracker_binary_age],
          [m4_eval(100 * tracker_minor_version + tracker_micro_version)])
m4_define([tracker_version],
          [tracker_major_version.tracker_minor_version.tracker_micro_version])
# This is the X.Y used in -llibtracker-FOO-X.Y
m4_define([tracker_api_version], [1.0])

# libtool version related macros
m4_define([tracker_lt_release], [tracker_major_version.tracker_minor_version])
m4_define([tracker_lt_current],
          [m4_eval(100 * tracker_minor_version + tracker_micro_version - tracker_interface_age)])
m4_define([tracker_lt_revision], [tracker_interface_age])
m4_define([tracker_lt_age], [m4_eval(tracker_binary_age - tracker_interface_age)])
m4_define([tracker_lt_current_minus_age],
          [m4_eval(tracker_lt_current - tracker_lt_age)])

# Define a string for the earliest version that this release has
# backwards binary compatibility with for all interfaces a module
# might. Unless we add module-only API with lower stability
# guarantees, this should be unchanged until we break binary compat
# for Tracker.
#
# TRACKER_BINARY_VERSION=$TRACKER_MAJOR_VERSION.$TRACKER_MINOR_VERSION.$LT_CURRENT
m4_define([tracker_binary_version], [1.0.0])

AC_INIT([tracker],
        [tracker_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=tracker],
        [tracker],
        [https://wiki.gnome.org/Projects/Tracker])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/tracker-store/tracker-main.vala])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Available since autoconf 2.60, implies _GNU_SOURCE and extensions if available.
# See: https://www.gnu.org/software/autoconf/manual/autoconf-2.64/html_node/Posix-Variants.html
AC_USE_SYSTEM_EXTENSIONS

# Adding the 'subdir-objects' Automake option here will fix some Automake
# warnings, but it will also break the build. I believe that it triggers
# <http://debbugs.gnu.org/cgi/bugreport.cgi?bug=13928>. That bug will
# be fixed in Automake 1.16, so we should be eventually able to use
# 'subdir-objects' (which is apparently will be on by default in Automake 2.0),
# but we'll need to require Automake 1.16 before we can do so.
AM_INIT_AUTOMAKE([1.11 no-define tar-ustar -Wno-portability no-dist-gzip dist-xz])

AC_PROG_LIBTOOL

AM_SILENT_RULES([yes])

dnl http://people.gnome.org/~walters/docs/build-api.txt
dnl We don't support srcdir != builddir.
echo \#buildapi-variable-no-builddir >/dev/null

IDT_COMPILE_WARNINGS

TRACKER_MAJOR_VERSION=tracker_major_version
TRACKER_MINOR_VERSION=tracker_minor_version
TRACKER_MICRO_VERSION=tracker_micro_version
TRACKER_INTERFACE_AGE=tracker_interface_age
TRACKER_BINARY_AGE=tracker_binary_age
TRACKER_VERSION=tracker_version
TRACKER_API_VERSION=tracker_api_version
TRACKER_BINARY_VERSION=tracker_binary_version

dnl Note: this creates TRACKER_API_VERSION_UNDERSCORES and AC_SUBST's it
AX_DOTS_TO_UNDERSCORES(TRACKER_API_VERSION, tracker_api_version)

AC_SUBST(TRACKER_MAJOR_VERSION)
AC_SUBST(TRACKER_MINOR_VERSION)
AC_SUBST(TRACKER_MICRO_VERSION)
AC_SUBST(TRACKER_INTERFACE_AGE)
AC_SUBST(TRACKER_BINARY_AGE)
AC_SUBST(TRACKER_API_VERSION)
AC_SUBST(TRACKER_VERSION)
AC_SUBST(TRACKER_BINARY_VERSION)

AC_DEFINE(TRACKER_MAJOR_VERSION, [tracker_major_version],
          [Define to the Tracker major version])
AC_DEFINE(TRACKER_MINOR_VERSION, [tracker_minor_version],
          [Define to the Tracker minor version])
AC_DEFINE(TRACKER_MICRO_VERSION, [tracker_micro_version],
          [Define to the Tracker micro version])
AC_DEFINE(TRACKER_INTERFACE_AGE, [tracker_interface_age],
          [Define to the Tracker interface age])
AC_DEFINE(TRACKER_BINARY_AGE, [tracker_binary_age],
          [Define to the Tracker binary age])

# libtool versioning
LT_RELEASE=tracker_lt_release
LT_CURRENT=tracker_lt_current
LT_REVISION=tracker_lt_revision
LT_AGE=tracker_lt_age
LT_CURRENT_MINUS_AGE=tracker_lt_current_minus_age
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])

AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h locale.h stddef.h stdint.h stdlib.h string.h strings.h sys/file.h sys/ioctl.h sys/param.h sys/statfs.h sys/statvfs.h sys/time.h unistd.h])

AC_CHECK_HEADER([zlib.h],
                [],
                [AC_MSG_ERROR([You must have zlib.h and zlib installed])])

AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Can posix_fadvise be used
AC_CHECK_DECLS(posix_fadvise, [], [], [
#define _XOPEN_SOURCE 600
#include <fcntl.h>])

# Checks for functions
AC_CHECK_FUNCS([posix_fadvise])
AC_CHECK_FUNCS([getline strnlen])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_CHECK_FUNCS([alarm ftruncate memchr memmove memset modf munmap setlocale sqrt strcasecmp strchr strcspn strndup strrchr strstr strtoul strtoull tzset])

# if statvfs64() is available, enable the 64-bit API extensions
AC_CHECK_FUNCS([statvfs64], [have_statvfs64=yes], [have_statvfs64=no])
if test "x$have_statvfs" = "xyes" ; then
   CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
fi

# Check for defines we expect
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
#include "stdio.h"
printf ("%s", __FUNCTION__);
   ]])],
   [have_function=yes],
   [have_function=no])

if test "x$have_function" = "xno" ; then
   AC_DEFINE(__FUNCTION__,
             __func__,
            [Defined for compilers not supporting __FUNCTION__])
fi

# Remember CFLAGS upon entering configure...
CFLAGS="$CFLAGS"

# Initialize GLib
GLIB_GSETTINGS
GLIB_TESTS

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Binary required versions
PYTHON_REQUIRED=2.6

# Library required versions
DBUS_REQUIRED=1.3.1
GLIB_REQUIRED=2.40.0
GTK_REQUIRED=3.0.0
LIBXML2_REQUIRED=2.6
HAL_REQUIRED=0.5
UPOWER_REQUIRED=0.9.0
MEEGOTOUCH_REQUIRED=0.20
POPPLER_REQUIRED=0.16.0
CAIRO_REQUIRED=1.0
LIBICU_REQUIRED=4.8.1.1
LIBJPEG_REQUIRED="any version"
LIBGIF_REQUIRED="any version"
LIBTIFF_REQUIRED="any version"
LIBVORBIS_REQUIRED=0.22
LIBFLAC_REQUIRED=1.2.1
LIBEXIF_REQUIRED=0.6
LIBGSF_REQUIRED=1.14.24
LIBOSINFO_REQUIRED=0.2.9
EXEMPI_REQUIRED=2.1.0
EVO_REQUIRED=2.32.0
EVO_SHELL_REQUIRED=2.32.0
EDS_REQUIRED=2.32.0
CAMEL_REQUIRED=2.32.0
GEE_REQUIRED=0.3
TAGLIB_REQUIRED=1.6
LIBGRSS_REQUIRED=0.7
NETWORK_MANAGER_REQUIRED=0.8
GSTREAMER_REQUIRED=0.10.31
GUPNP_DLNA_REQUIRED=0.9.4
LIBPNG_REQUIRED=0.89
LIBMEDIAART_REQUIRED=1.9.0

# 3.6.11 for sqlite_backup API
# 3.6.16 to fix test failures
# 3.6.17 for shared cache mode with virtual tables
# 3.7.0 for WAL
# 3.7.9 for FTS4 content= support
# 3.7.15 for sqlite3_errstr() support
SQLITE_REQUIRED=3.7.15

# Needed to generate .gir files,
# see http://live.gnome.org/GnomeGoals/AddGObjectIntrospectionSupport
GOBJECT_INTROSPECTION_CHECK([0.9.5])

GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
AC_SUBST(GLIB_PREFIX)

# Check for libpng 1.2 or higher
PKG_CHECK_MODULES(LIBPNG, [libpng >= 1.2])
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBPNG_LIBS)

# Check requirements for gvdb
GVDB_REQUIRED="glib-2.0 >= $GLIB_REQUIRED"
PKG_CHECK_MODULES(GVDB, [$GVDB_REQUIRED])

# Check requirements for libtracker-bus
LIBTRACKER_BUS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                         gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_BUS, [$LIBTRACKER_BUS_REQUIRED])

# Check requirements for libtracker-common
LIBTRACKER_COMMON_REQUIRED="glib-2.0        >= $GLIB_REQUIRED
                            gio-unix-2.0    >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_COMMON, [$LIBTRACKER_COMMON_REQUIRED])

case $host in
  *-*-openbsd*)
    LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS -lkvm"
    ;;
esac

# Check requirements for libtracker-data
LIBTRACKER_DATA_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                          gio-unix-2.0 >= $GLIB_REQUIRED
                          sqlite3      >= $SQLITE_REQUIRED
                          uuid"
PKG_CHECK_MODULES(LIBTRACKER_DATA, [$LIBTRACKER_DATA_REQUIRED])

LIBTRACKER_DATA_LIBS="$LIBTRACKER_DATA_LIBS -lz -lm"

# Check requirements for libtracker-direct
LIBTRACKER_DIRECT_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_DIRECT, [$LIBTRACKER_DIRECT_REQUIRED])

# Check requirements for libtracker-extract
LIBTRACKER_EXTRACT_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                             gio-unix-2.0 >= $GLIB_REQUIRED
			     gmodule-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_EXTRACT, [$LIBTRACKER_EXTRACT_REQUIRED])

# Check requirements for libtracker-fts
LIBTRACKER_FTS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                         gio-unix-2.0 >= $GLIB_REQUIRED
                         sqlite3      >= $SQLITE_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_FTS, [$LIBTRACKER_FTS_REQUIRED])

# Check requirements for libtracker-miner
LIBTRACKER_MINER_REQUIRED="glib-2.0        >= $GLIB_REQUIRED
                           gio-unix-2.0    >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_MINER, [$LIBTRACKER_MINER_REQUIRED])

# Check requirements for libtracker-control
LIBTRACKER_CONTROL_REQUIRED="glib-2.0        >= $GLIB_REQUIRED
                             gio-unix-2.0    >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_CONTROL, [$LIBTRACKER_CONTROL_REQUIRED])

# Check requirements for libtracker-sparql
LIBTRACKER_SPARQL_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED
                            uuid"

PKG_CHECK_MODULES(LIBTRACKER_SPARQL, [$LIBTRACKER_SPARQL_REQUIRED])

# Check requirements for tracker-control
TRACKER_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                  gio-unix-2.0 >= $GLIB_REQUIRED
                  uuid"

PKG_CHECK_MODULES(TRACKER, [$TRACKER_REQUIRED])

# Check requirements for tracker-resdump
TRACKER_RESDUMP_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                          gee-0.8      >= $GEE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_RESDUMP, [$TRACKER_RESDUMP_REQUIRED],
                  [have_tracker_resdump=yes],
                  [have_tracker_resdump=no])

# Check requirements for tracker-store
TRACKER_STORE_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                        gio-unix-2.0 >= $GLIB_REQUIRED
                        gmodule-2.0  >= $GLIB_REQUIRED
                        sqlite3      >= $SQLITE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_STORE, [$TRACKER_STORE_REQUIRED])

TRACKER_STORE_LIBS="$TRACKER_STORE_LIBS -lz -lm"

# Check requirements for tracker-extract
TRACKER_EXTRACT_REQUIRED="glib-2.0        >= $GLIB_REQUIRED
                          gio-unix-2.0    >= $GLIB_REQUIRED
                          gmodule-2.0     >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_EXTRACT, [$TRACKER_EXTRACT_REQUIRED])

TRACKER_EXTRACT_MODULES_DIR="${libdir}/tracker-${TRACKER_API_VERSION}/extract-modules"
AC_SUBST(TRACKER_EXTRACT_MODULES_DIR)

# NOTE: We don't use ${TRACKER_API_VERSION} because other content like
# the ontology is installed to the same location.
TRACKER_EXTRACT_RULES_DIR="${datadir}/tracker/extract-rules"
AC_SUBST(TRACKER_EXTRACT_RULES_DIR)

# Check requirements for tracker-extract modules
TRACKER_EXTRACT_MODULES_REQUIRED="glib-2.0     >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_EXTRACT_MODULES, [$TRACKER_EXTRACT_MODULES_REQUIRED])

# Check requirements for tracker-writeback
TRACKER_WRITEBACK_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED
                            gmodule-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_WRITEBACK, [$TRACKER_WRITEBACK_REQUIRED])

# Check requirements for tracker-miner-fs
TRACKER_MINER_FS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                           gio-unix-2.0 >= $GLIB_REQUIRED
                           gmodule-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_FS, [$TRACKER_MINER_FS_REQUIRED])

TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS -lz -lm"

# Check requirements for tracker-miner-apps
TRACKER_MINER_APPS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                             gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_APPS, [$TRACKER_MINER_APPS_REQUIRED])

# Check requirements for tracker-miner-user-guides
TRACKER_MINER_USER_GUIDES_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                                    gio-unix-2.0 >= $GLIB_REQUIRED
                                    libxml-2.0   >= $LIBXML2_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_USER_GUIDES, [$TRACKER_MINER_USER_GUIDES_REQUIRED])

# Check requirements for tracker-miner-evolution
evolution_plugin_name=""

# First test for 3_3_5
TRACKER_MINER_EVOLUTION_3_3_5_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                        evolution-shell-3.0       >= 3.1
                                        evolution-plugin-3.0
                                        libemail-utils
                                        libemail-engine
                                        evolution-data-server-1.2 >= $EDS_REQUIRED
                                        camel-1.2                 >= $CAMEL_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_3_3_5_REQUIRED],
                  [have_tracker_miner_evolution=yes],
                  [have_tracker_miner_evolution=no])

if test -z "$evolution_plugin_name" && test "x$have_tracker_miner_evolution" = "xyes"; then
   evolution_plugin_name="evolution-plugin-3.0"

   AC_DEFINE(EVOLUTION_SHELL_3_3_5, 1, [Use new evolution-shell API])
   AC_DEFINE(EVOLUTION_SHELL_3_2, 1, [Use new evolution-shell API])
   AC_DEFINE(EVOLUTION_SHELL_2_91, 1, [Use new evolution-shell API])
fi

# Second test for 3_2
TRACKER_MINER_EVOLUTION_3_2_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                      evolution-shell-3.0       >= 3.1
                                      evolution-plugin-3.0
                                      evolution-data-server-1.2 >= $EDS_REQUIRED
                                      camel-1.2                 >= $CAMEL_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_3_2_REQUIRED],
                  [have_tracker_miner_evolution=yes],
                  [have_tracker_miner_evolution=no])

if test -z "$evolution_plugin_name" && test "x$have_tracker_miner_evolution" = "xyes"; then
   evolution_plugin_name="evolution-plugin-3.0"

   AC_DEFINE(EVOLUTION_SHELL_3_2, 1, [Use new evolution-shell API])
   AC_DEFINE(EVOLUTION_SHELL_2_91, 1, [Use new evolution-shell API])
fi

# Third test for 2_91
TRACKER_MINER_EVOLUTION_2_91_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                       evolution-shell-3.0
                                       evolution-plugin-3.0
                                       evolution-data-server-1.2 >= $EDS_REQUIRED
                                       camel-1.2                 >= $CAMEL_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_2_91_REQUIRED],
                  [have_tracker_miner_evolution=yes],
                  [have_tracker_miner_evolution=no])

if test -z "$evolution_plugin_name" && test "x$have_tracker_miner_evolution" = "xyes"; then
   evolution_plugin_name="evolution-plugin-3.0"
   AC_DEFINE(EVOLUTION_SHELL_2_91, 1, [Use new evolution-shell API])
fi

# Fourth test others (do we really want support for anything < 3.2)?
TRACKER_MINER_EVOLUTION_LAST_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                       evolution-shell           >= $EVO_SHELL_REQUIRED
                                       evolution-plugin          >= $EVO_REQUIRED
                                       evolution-data-server-1.2 >= $EDS_REQUIRED
                                       camel-1.2                 >= $CAMEL_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_LAST_REQUIRED],
                  [have_tracker_miner_evolution=yes],
                  [have_tracker_miner_evolution=no])

if test -z "$evolution_plugin_name" && test "x$have_tracker_miner_evolution" = "xyes"; then
   evolution_plugin_name="evolution-plugin"
fi

# Check requirements for tracker-miner-thunderbird
TRACKER_MINER_THUNDERBIRD_REQUIRED=5.0
have_tracker_miner_thunderbird="no "

AC_PATH_PROG(THUNDERBIRD, thunderbird, thunderbird)
if test -n $THUNDERBIRD; then
   thunderbird_version=`$THUNDERBIRD --version 2>/dev/null | awk '{print $NF}'`

   AX_COMPARE_VERSION([$thunderbird_version], ge, [$TRACKER_MINER_THUNDERBIRD_REQUIRED],
                      have_tracker_miner_thunderbird="yes",
                      have_tracker_miner_thunderbird="no ")
   AC_SUBST(THUNDERBIRD)
fi

# Check requirements for tracker-miner-firefox
TRACKER_MINER_FIREFOX_REQUIRED=4.0
have_tracker_miner_firefox="no "

AC_PATH_PROG(FIREFOX, firefox, firefox)
if test -n $FIREFOX; then
   firefox_version=`$FIREFOX --version 2>/dev/null | awk '{print $NF}'`

   AX_COMPARE_VERSION([$firefox_version], ge, [$TRACKER_MINER_FIREFOX_REQUIRED],
                      have_tracker_miner_firefox="yes",
                      have_tracker_miner_firefox="no ")
   AC_SUBST(FIREFOX)
fi

# Check requirements for tracker-miner-rss
TRACKER_MINER_RSS_REQUIRED="glib-2.0 >= $GLIB_REQUIRED
                            libgrss  >= $LIBGRSS_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_RSS, [$TRACKER_MINER_RSS_REQUIRED],
                  [have_tracker_miner_rss=yes],
                  [have_tracker_miner_rss=no])

# Check requirements for tracker-needle
TRACKER_NEEDLE_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                         gio-unix-2.0 >= $GLIB_REQUIRED
                         gmodule-2.0  >= $GLIB_REQUIRED
                         gtk+-3.0     >= $GTK_REQUIRED"

PKG_CHECK_MODULES(TRACKER_NEEDLE, [$TRACKER_NEEDLE_REQUIRED],
                  [have_tracker_needle=yes],
                  [have_tracker_needle=no])

# Check requirements for tracker-preferences
TRACKER_PREFERENCES_REQUIRED="glib-2.0       >= $GLIB_REQUIRED
                              gio-unix-2.0   >= $GLIB_REQUIRED
                              gmodule-2.0    >= $GLIB_REQUIRED
                              gtk+-3.0       >= $GTK_REQUIRED"

PKG_CHECK_MODULES(TRACKER_PREFERENCES, [$TRACKER_PREFERENCES_REQUIRED],
                  [have_tracker_preferences=yes],
                  [have_tracker_preferences=no])

# Check requirements for Nautilus extension
TRACKER_NAUTILUS_EXTENSION_REQUIRED="libnautilus-extension"

PKG_CHECK_MODULES(TRACKER_NAUTILUS_EXTENSION, [$TRACKER_NAUTILUS_EXTENSION_REQUIRED],
                  [have_tracker_nautilus_extension=yes],
                  [have_tracker_nautilus_extension=no])

# Check we have Vala valac command we need
VALA_MIN_VERSION=0.18.0
AM_PROG_VALAC([$VALA_MIN_VERSION])
if test "x$VALAC" = "x"; then
    AC_MSG_ERROR([Vala requested but valac >= $VALA_MIN_VERSION is not installed])
fi

####################################################################
# Check gettext/intltool support
####################################################################
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define the gettext package to be used])
IT_PROG_INTLTOOL([0.40.0])

####################################################################
# Check if we should enable GCOV coverage reporting support
####################################################################

AC_ARG_ENABLE(gcov,
              AS_HELP_STRING([--enable-gcov],
                             [enable coverage reporting support [[default=no]]]),
              [enable_gcov=$enableval],
              [enable_gcov=no])
AM_CONDITIONAL(ENABLE_GCOV, test "x$enable_gcov" != "xno")

if test "x$enable_gcov" != "xno" ; then
  GCOV_VALAFLAGS="-g"
  GCOV_CFLAGS="-ftest-coverage -fprofile-arcs"
  GCOV_LIBS="-lgcov"
else
  GCOV_VALAFLAGS=
  GCOV_CFLAGS=
  GCOV_LIBS=
fi

AC_SUBST(GCOV_VALAFLAGS)
AC_SUBST(GCOV_CFLAGS)
AC_SUBST(GCOV_LIBS)

####################################################################
# General VALAFLAGS/CFLAGS/LIBS
#
# NOTE: BUILD_VALACFLAGS are for the sources built by Vala,
#       not for .vala files themselves.
####################################################################
BUILD_VALAFLAGS="-g --target-glib=${GLIB_REQUIRED}"
BUILD_VALACFLAGS="${WARN_VALACFLAGS} ${GCOV_CFLAGS} -DG_LOG_DOMAIN=\\\"Tracker\\\" -DTRACKER_COMPILATION"
BUILD_CFLAGS="${WARN_CFLAGS} ${GCOV_CFLAGS} -DG_LOG_DOMAIN=\\\"Tracker\\\" -DTRACKER_COMPILATION"
BUILD_LIBS="${GCOV_LIBS}"

AC_SUBST(BUILD_VALAFLAGS)
AC_SUBST(BUILD_VALACFLAGS)
AC_SUBST(BUILD_CFLAGS)
AC_SUBST(BUILD_LIBS)

####################################################################
# Check if linker supports --enable-new-dtags
####################################################################
AC_MSG_CHECKING([whether $CC supports -Wl,--enable-new-dtags])
save_LDFLAGS=$LDFLAGS
LDFLAGS="-Wl,--enable-new-dtags $LDFLAGS"

AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
             [enable_new_dtags=yes; AC_MSG_RESULT(yes)], [enable_new_dtags=no; AC_MSG_RESULT(no)])
LDFLAGS=$save_LDFLAGS
if test "x$enable_new_dtags" = "xyes"; then
    LDFLAGS="-Wl,--enable-new-dtags $LDFLAGS"
fi

####################################################################
# Check for minimal
####################################################################

AC_ARG_ENABLE(minimal,
              AS_HELP_STRING([--enable-minimal],
                             [disabled EVERYTHING except the very core (tracker-store and foundation libraries) [[default=no]]]),
              [enable_minmal=$enableval],
              [enable_minmal=no])

# WE only disable the binaries we're building here, not optional
# extras for those binaries
if test "x$enable_minimal" = "xyes"; then
   AC_MSG_WARN([Building minimal configuration])

   enable_tracker_extract=no
   enable_tracker_writeback=no
   enable_miner_fs=no
   enable_miner_apps=no
   enable_miner_user_guides=no
   enable_miner_rss=no
   enable_miner_evolution=no
   enable_miner_thunderbird=no
   enable_miner_firefox=no
   enable_nautilus_extension=no
   enable_tracker_needle=no
   enable_tracker_preferences=no
fi

####################################################################
# Check for functional-test include/exclude
####################################################################
AC_ARG_ENABLE([functional-tests],
              AS_HELP_STRING([--enable-functional-tests],
                             [enable functional tests [[default=no]]]),
              [enable_functional_tests=$enableval],
              [enable_functional_tests=no])

if test x$enable_functional_tests != "xno"; then
   # Python check, require >= 2.6
   AC_PATH_PROG([PYTHON],[python],[:])
   AS_IF([test "$PYTHON" != ":"],
         [AM_PYTHON_CHECK_VERSION([$PYTHON],[$PYTHON_REQUIRED],[:],[PYTHON=":"])])

   if test "$PYTHON" = ":"; then
      AC_MSG_ERROR([Could not find Python >= $PYTHON_REQUIRED (for functional-tests, try --disable-functional-tests).])
   fi

   AC_DEFINE(DIST_FUNCTIONAL_TESTS, 1, [Include functional tests in the installation])
fi

AM_CONDITIONAL(DIST_FUNCTIONAL_TESTS, test "x$enable_functional_tests" != "xno")

####################################################################
# Check for gtk-doc and docbook-tools
####################################################################

# Check for GTK_DOC_CHECK availability. The GTK_DOC_CHECK invocation
# must be on its own line, gtkdocize relies on it
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.8])

# NOTE: We need to use a separate automake conditional for this
#       to make this work with the tarballs.
AM_CONDITIONAL([ENABLE_GTK_DOC], test "x$enable_gtk_doc" = xyes)
],
[
AM_CONDITIONAL([ENABLE_GTK_DOC], false)
])

####################################################################
# Check if we should install Windows specific binaries
####################################################################

AC_MSG_CHECKING(for WIN32)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
   #ifndef WIN32
   #error
   #endif
   ]])],
   [native_win32=yes; AC_MSG_RESULT(yes)],
   [native_win32=no; AC_MSG_RESULT(no)])

AM_CONDITIONAL(OS_WIN32, test "$native_win32" = "yes")

if test "$native_win32" = "yes" ; then
   AC_DEFINE(OS_WIN32, 1, [Define if we are on win32])
fi

####################################################################
# Check if we should install Maemo/MeeGo specific ontologies
####################################################################

AC_ARG_ENABLE(maemo,
              AS_HELP_STRING([--enable-maemo],
                             [enable maemo ontology [[default=no]]]),
              [enable_maemo=$enableval],
              [enable_maemo=no])

AM_CONDITIONAL(HAVE_MAEMO, test "x$enable_maemo" = "xyes")

if test "x$enable_maemo" = "xyes" ; then
   AC_DEFINE(HAVE_MAEMO, 1, [Define if we enable Maemo specific features])
fi

####################################################################
# Check if we should disable the journal
####################################################################

AC_ARG_ENABLE(journal,
              AS_HELP_STRING([--enable-journal],
                             [enable database journal backup mechanism [[default=yes]]]),
              [enable_journal=$enableval],
              [enable_journal=yes])

if test "x$enable_journal" != "xno" ; then
   have_tracker_journal="yes"
else
   have_tracker_journal="no  (disabled)"
fi

AM_CONDITIONAL(DISABLE_JOURNAL, test "x$enable_journal" = "xno")

if test "x$enable_journal" = "xno" ; then
   AC_DEFINE(DISABLE_JOURNAL, 1, [Define if we disable the journal])
fi

####################################################################
# Check for SQLite
####################################################################

PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= $SQLITE_REQUIRED])
AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)

# Make sure SQLite is compiled thread-safe
AX_SQLITE_THREADSAFE
if test "x$ax_cv_sqlite_threadsafe" != "xyes"; then
  AC_MSG_ERROR([sqlite3 is not compiled thread-safe])
fi

# Make sure we're not using versions which cause problems.
#
# Avoid versions:
#   3.7.10 - 3.7.13: https://mail.gnome.org/archives/tracker-list/2012-October/msg00028.html
#   3.8.1:           https://mail.gnome.org/archives/tracker-list/2013-November/msg00021.html
#   3.8.4.1:         https://mail.gnome.org/archives/tracker-list/2014-April/msg00001.html
#
AC_MSG_CHECKING(whether SQLite3 version is safe)

PKG_CHECK_EXISTS([sqlite3 <= 3.7.9], sqlite_safe=yes, sqlite_safe=no)
if test "x$sqlite_safe" != "xyes"; then
   PKG_CHECK_EXISTS([sqlite3 >= 3.7.14], sqlite_safe=yes, sqlite_safe=no)
fi

if test "x$sqlite_safe" = "xyes"; then
   PKG_CHECK_EXISTS([sqlite3 = 3.8.1], sqlite_safe=no, sqlite_safe=yes)
fi

if test "x$sqlite_safe" = "xyes"; then
   PKG_CHECK_EXISTS([sqlite3 = 3.8.4.2], sqlite_safe=no, sqlite_safe=yes)
fi


if test "x$sqlite_safe" != "xyes"; then
   AC_MSG_RESULT(no)
else
   AC_MSG_RESULT(yes)
fi

##################################################################
# Check for libtracker-common, make sure libstemmer exists
##################################################################

AC_ARG_ENABLE([libstemmer],
              AS_HELP_STRING([--enable-libstemmer],
                             [enable the stemming while indexing [[default=auto]]]),
              [enable_libstemmer=$enableval],
              [enable_libstemmer=auto])

if test "x$enable_libstemmer" != "xno" ; then
   AC_CHECK_LIB([stemmer], [sb_stemmer_new], [have_libstemmer=yes], [have_libstemmer=no])

   if test "x$have_libstemmer" = "xyes"; then
      AC_DEFINE(HAVE_LIBSTEMMER, [], [Define if we have libstemmer])
      LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS -lstemmer"
   fi
else
   have_libstemmer="no  (disabled)"
fi

if test "x$enable_libstemmer" = "xyes"; then
   if test "x$have_libstemmer" != "xyes"; then
      AC_MSG_ERROR([Could not find libstemmer.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBSTEMMER, test "x$have_libstemmer" = "xyes")

##################################################################
# Check for tracker-fts, allow disabling FTS support
##################################################################

AC_ARG_ENABLE([tracker-fts],
              AS_HELP_STRING([--enable-tracker-fts],
                             [enable the tracker FTS [[default=auto]]]),
              [enable_tracker_fts=$enableval],
              [enable_tracker_fts=auto])

if test "x$enable_tracker_fts" = "xno" ; then
   have_tracker_fts="no  (disabled)"
else
   have_tracker_fts="yes"
fi

if test "x$have_tracker_fts" = "xyes"; then
   AC_DEFINE(HAVE_TRACKER_FTS, [1], [Define to 1 if tracker FTS is compiled])

   AX_SQLITE_BUILTIN_FTS5
   if test "x$ax_cv_sqlite_builtin_fts5" = "xyes" ; then
      have_builtin_fts5="yes"
      AC_DEFINE(HAVE_BUILTIN_FTS, [], [Defined if Sqlite has FTS5 compiled in])
   else
      have_builtin_fts5="no"

      # Make sure SQLite has extension loading enabled
      AX_SQLITE_AUTO_EXTENSION
      if test "x$ax_cv_sqlite_auto_extension" != "xyes"; then
        AC_MSG_ERROR([sqlite3 cannot load extensions])
      fi
   fi
else
   AC_DEFINE(HAVE_TRACKER_FTS, [0], [Define to 0 if tracker FTS is not compiled])
   have_builtin_fts5="disabled"
fi

AM_CONDITIONAL(HAVE_BUILTIN_FTS, test "$have_builtin_fts5" = "yes")
AM_CONDITIONAL(HAVE_TRACKER_FTS, test "$have_tracker_fts" = "yes")

####################################################################
# bash-completion
####################################################################

AC_ARG_WITH([bash-completion-dir],
    AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
        [Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
    [],
    [with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
   PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
       [BASH_COMPLETION_DIR=$($PKG_CONFIG --variable=completionsdir bash-completion)],
       [BASH_COMPLETION_DIR="${datadir}/bash-completion/completions"])
elif test "x$with_bash_completion_dir" != "xno"; then
   # Set with_bash_completions_dir to avoid printing path twice below
   BASH_COMPLETION_DIR="$with_bash_completion_dir"
   with_bash_completion_dir="yes"
fi

AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([WITH_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])

####################################################################
# Check for D-Bus requirements
####################################################################

# Check we are not using a different D-Bus services dir
AC_ARG_WITH([session_bus_services_dir],
            AS_HELP_STRING([--with-session-bus-services-dir],
                           [path to DBus services directory]),
            [dbus_services_dir=$withval],
            [dbus_services_dir=$datadir/dbus-1/services])

DBUS_SERVICES_DIR="$dbus_services_dir"
AC_SUBST(DBUS_SERVICES_DIR)

####################################################################
# GLib/GIO utilities
####################################################################

GLIB_MKENUMS=`$PKG_CONFIG glib-2.0 --variable=glib_mkenums`
AC_SUBST(GLIB_MKENUMS)

GDBUS_CODEGEN=`$PKG_CONFIG --variable=gdbus_codegen gio-2.0`
AC_SUBST(GDBUS_CODEGEN)

####################################################################
# Should we build GLib based unit tests
####################################################################

AC_ARG_ENABLE(unit_tests,
              AS_HELP_STRING([--enable-unit-tests],
                             [enable unit tests [[default=yes]]]),
              [enable_unit_tests=$enableval],
              [enable_unit_tests=yes])

if test "x$enable_unit_tests" != "xno" ; then
   have_unit_tests=yes

   AC_DEFINE(HAVE_UNIT_TEST, 1, [Define if we have GLib unit test framework])

   AC_PATH_PROG(gtester, gtester)
   if test -z $gtester; then
      AC_MSG_ERROR([Could not find 'gtester'])
   fi
else
   have_unit_tests=no
fi

AM_CONDITIONAL(HAVE_UNIT_TESTS, test "x$have_unit_tests" = "xyes")

##################################################################
# Check for libtracker-common: upower for battery/power support
##################################################################

AC_ARG_ENABLE(upower,
              AS_HELP_STRING([--disable-upower],
                             [disable UPower support for AC power detection [[default=auto]]]),
              [enable_upower=$enableval],
              [enable_upower=auto])

AS_IF([test "x$enable_upower" != "xno"], [
   PKG_CHECK_MODULES(UPOWER,
                     [upower-glib >= $UPOWER_REQUIRED],
                     [have_upower=yes],
                     [have_upower=no])

   TRACKER_MINER_FS_CFLAGS="$TRACKER_MINER_FS_CFLAGS $UPOWER_CFLAGS"
   TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS $UPOWER_LIBS"
   save_LIBS=$LIBS
   LIBS=$UPOWER_LIBS
   AC_CHECK_FUNCS(up_client_get_on_low_battery)
   LIBS=$save_LIBS

   if test "x$have_upower" = "xyes"; then
      AC_DEFINE(HAVE_UPOWER, [], [Define if we have UPOWER])
   fi
], [
   have_upower="no  (disabled)"
])

if test "x$enable_upower" = "xyes"; then
   if test "x$have_upower" != "xyes"; then
      AC_MSG_ERROR([Could not find upower >= $UPOWER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_UPOWER, test "x$have_upower" = "xyes")

##################################################################
# Check for libtracker-common: hal for battery/power support (old)
##################################################################

AC_ARG_ENABLE(hal,
              AS_HELP_STRING([--disable-hal],
                             [disable HAL support for AC power detection [[default=auto]]]),
              [enable_hal=$enableval],
              [enable_hal=auto])

if test "x$have_upower" != "xyes" && test "x$enable_hal" != "xno"; then
   PKG_CHECK_MODULES(HAL,
                     [dbus-1      >= $DBUS_REQUIRED
                      hal         >= $HAL_REQUIRED],
                     [have_hal=yes] ,
                     [have_hal=no])

   TRACKER_MINER_FS_CFLAGS="$TRACKER_MINER_FS_CFLAGS $HAL_CFLAGS"
   TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS $HAL_LIBS"

   if test "x$have_hal" = "xyes"; then
      AC_DEFINE(HAVE_HAL, [], [Define if we have HAL])
   fi
else
   have_hal="no  (disabled)"
fi

if test "x$enable_hal" = "xyes"; then
   if test "x$have_upower" = "xyes"; then
      AC_MSG_ERROR([Only one of UPower and HAL can be used.])
   elif test "x$have_hal" != "xyes"; then
      AC_MSG_ERROR([Could not find hal >= $HAL_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_HAL, test "x$have_hal" = "xyes")

##################################################################
# Check for libtracker-data and libtracker-fts: Unicode support
#
# By default, AUTO with this order of preference:
#  1)  libunistring
#  2)  libicu
##################################################################

# Check for libunistring...
AC_CHECK_HEADER(uniwbrk.h,
                AC_CHECK_LIB([unistring],
                             [u8_wordbreaks],
                             [have_libunistring=yes],
                             [have_libunistring=no]))
LIBUNISTRING_CFLAGS=""
LIBUNISTRING_LIBS="-lunistring"

# Check for libicu...
PKG_CHECK_MODULES(LIBICU,
                  [icu-i18n >= $LIBICU_REQUIRED icu-uc >= $LIBICU_REQUIRED],
                  [have_libicu=yes],
                  [have_libicu=no])

# Configure option
AC_ARG_WITH([unicode-support],
            AS_HELP_STRING([--with-unicode-support],
                           [Unicode support library? (libunistring|libicu) [[default=auto]]]),
            [with_unicode_support=$withval],
            [with_unicode_support=auto])

# If auto, decide ourselves
if test "x$with_unicode_support" = "xauto"; then
   if test "x$have_libicu" = "xyes"; then
      with_unicode_support=libicu
   else
      if test "x$have_libunistring" = "xyes"; then
         with_unicode_support=libunistring
      else
         AC_MSG_ERROR([Could not find either libunistring or libicu])
      fi
   fi
fi

UNICODE_SUPPORT_CFLAGS=
UNICODE_SUPPORT_LIBS=

case "x$with_unicode_support" in
     # Use libunistring
     "xlibunistring")
        UNICODE_SUPPORT_CFLAGS=$LIBUNISTRING_CFLAGS
        UNICODE_SUPPORT_LIBS=$LIBUNISTRING_LIBS
        if test "x$have_libunistring" = "xyes"; then
           AC_DEFINE(HAVE_LIBUNISTRING, [1], [libunistring Unicode support library])
        else
           AC_MSG_ERROR([Could not find libunistring])
        fi
     ;;
     # Use libicu
     "xlibicu")
        UNICODE_SUPPORT_CFLAGS=$LIBICU_CFLAGS
        UNICODE_SUPPORT_LIBS=$LIBICU_LIBS
        if test "x$have_libicu" = "xyes"; then
          AC_DEFINE(HAVE_LIBICU, [1], [libicu Unicode support library])
        else
          AC_MSG_ERROR([Could not find libicu])
        fi
     ;;
     # Invalid option value
     *) AC_MSG_ERROR([Wrong value for --with-unicode-support: $with_unicode_support]) ;;
esac

AM_CONDITIONAL(HAVE_LIBICU, test "x$have_libicu" = "xyes")

# Add to libtracker-common
LIBTRACKER_COMMON_CFLAGS="$LIBTRACKER_COMMON_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS $UNICODE_SUPPORT_LIBS"

# Add to libtracker-fts
LIBTRACKER_FTS_CFLAGS="$LIBTRACKER_FTS_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_FTS_LIBS="$LIBTRACKER_FTS_LIBS $UNICODE_SUPPORT_LIBS"

# Add to libtracker-data
LIBTRACKER_DATA_CFLAGS="$LIBTRACKER_DATA_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_DATA_LIBS="$LIBTRACKER_DATA_LIBS $UNICODE_SUPPORT_LIBS"

# Add to libtracker-extract
LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $UNICODE_SUPPORT_LIBS"

AM_CONDITIONAL(BUILD_LIBUNISTRING_PARSER, test "x$with_unicode_support" = "xlibunistring")
AM_CONDITIONAL(BUILD_LIBICU_PARSER, test "x$with_unicode_support" = "xlibicu")

##################################################################
# Check for libtracker-miner: network-manager for web miners
##################################################################

AC_ARG_ENABLE(network_manager,
              AS_HELP_STRING([--enable-network-manager],
                             [enable NetworkManager support to detect network status [[default=auto]]]),
              [enable_network_manager=$enableval],
              [enable_network_manager=auto])

if test "x$enable_network_manager" != "xno"; then
   PKG_CHECK_MODULES(NETWORK_MANAGER,
                     [libnm-glib >= $NETWORK_MANAGER_REQUIRED],
                     [have_network_manager=yes],
                     [have_network_manager=no])

   LIBTRACKER_MINER_CFLAGS="$LIBTRACKER_MINER_CFLAGS $NETWORK_MANAGER_CFLAGS"
   LIBTRACKER_MINER_LIBS="$LIBTRACKER_MINER_LIBS $NETWORK_MANAGER_LIBS"

   if test "x$have_network_manager" = "xyes"; then
      AC_DEFINE(HAVE_NETWORK_MANAGER, [], [Define if we have NetworkManager for network status detection])
   fi
fi

if test "x$enable_network_manager" = "xyes"; then
   if test "x$have_network_manager" != "xyes"; then
      AC_MSG_ERROR([Could not find libnm-glib >= $NETWORK_MANAGER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_NETWORK_MANAGER, test "x$have_network_manager" = "xyes")

##################################################################
# Check for libtracker-miner, tracker-extract: libmediaart
##################################################################

AC_ARG_ENABLE(libmediaart,
              AS_HELP_STRING([--enable-libmediaart],
                             [enable libmediaart for caching published video/audio art [[default=auto]]]),
              [enable_libmediaart=$enableval],
              [enable_libmediaart=auto])

if test "x$enable_libmediaart" != "xno" ; then
   PKG_CHECK_MODULES(LIBMEDIAART,
                     [libmediaart-2.0 >= $LIBMEDIAART_REQUIRED],
                     [have_libmediaart=yes],
                     [have_libmediaart=no])

   LIBTRACKER_MINER_CFLAGS="$LIBTRACKER_MINER_CFLAGS $LIBMEDIAART_CFLAGS"
   LIBTRACKER_MINER_LIBS="$LIBTRACKER_MINER_LIBS $LIBMEDIAART_LIBS"

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $LIBMEDIAART_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $LIBMEDIAART_LIBS"

   TRACKER_EXTRACT_CFLAGS="$TRACKER_EXTRACT_CFLAGS $LIBMEDIAART_CFLAGS"
   TRACKER_EXTRACT_LIBS="$TRACKER_EXTRACT_LIBS $LIBMEDIAART_LIBS"

   TRACKER_MINER_FS_CFLAGS="$TRACKER_MINER_FS_CFLAGS $LIBMEDIAART_CFLAGS"
   TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS $LIBMEDIAART_LIBS"

   # Used for .pc file...
   LIBTRACKER_MINER_PC_REQUIRES="libmediaart-2.0"
   AC_SUBST(LIBTRACKER_MINER_PC_REQUIRES)

   if test "x$have_libmediaart" = "xyes"; then
      AC_DEFINE(HAVE_LIBMEDIAART, [], [Define if we have libmediaart])
   fi
else
   have_libmediaart="no  (disabled)"
fi

if test "x$enable_libmediaart" = "xyes"; then
   if test "x$have_libmediaart" != "xyes"; then
      AC_MSG_ERROR([Could not find libmediaart >= $LIBMEDIAART_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBMEDIAART, test "x$have_libmediaart" = "xyes")

##################################################################
# Check for libtracker-extract: libexif
##################################################################

AC_ARG_ENABLE(libexif,
              AS_HELP_STRING([--enable-libexif],
                             [enable extractor for exif metadata [[default=auto]]]),
              [enable_libexif=$enableval],
              [enable_libexif=auto])

if test "x$enable_libexif" != "xno" ; then
   PKG_CHECK_MODULES(LIBEXIF,
                     [libexif >= $LIBEXIF_REQUIRED],
                     [have_libexif=yes],
                     [have_libexif=no])

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $LIBEXIF_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $LIBEXIF_LIBS"

   if test "x$have_libexif" = "xyes"; then
      AC_DEFINE(HAVE_LIBEXIF, [], [Define if we have libexif])
   fi
else
   have_libexif="no  (disabled)"
fi

if test "x$enable_libexif" = "xyes"; then
   if test "x$have_libexif" != "xyes"; then
      AC_MSG_ERROR([Could not find libexif >= $LIBEXIF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_EXIF, test "x$have_libexif" = "xyes")

##################################################################
# Check for libtracker-extract: libiptcdata
##################################################################

AC_ARG_ENABLE(libiptcdata,
              AS_HELP_STRING([--enable-libiptcdata],
                             [enable extractor for iptc metadata [[default=auto]]]),
              [enable_libiptcdata=$enableval],
              [enable_libiptcdata=auto])

if test "x$enable_libiptcdata" != "xno" ; then
   PKG_CHECK_MODULES(LIBIPTCDATA,
                     [libiptcdata],
                     [have_libiptcdata=yes],
                     [have_libiptcdata=no])

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $LIBIPTCDATA_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $LIBIPTCDATA_LIBS"

   if test "x$have_libiptcdata" = "xyes"; then
      AC_DEFINE(HAVE_LIBIPTCDATA, [], [Define if we have libiptcdata])
   fi
else
   have_libiptcdata="no  (disabled)"
fi

if test "x$enable_libiptcdata" = "xyes"; then
   if test "x$have_libiptcdata" != "xyes"; then
      AC_MSG_ERROR([Could not find libiptcdata.])
   fi
fi

AM_CONDITIONAL(HAVE_IPTC, test "x$have_libiptcdata" = "xyes")

##################################################################
# Check for libtracker-extract, tracker-writeback: exempi
##################################################################

AC_ARG_ENABLE(exempi,
              AS_HELP_STRING([--enable-exempi],
                             [enable extractor for XMP metadata [[default=auto]]]),
              [enable_exempi=$enableval],
              [enable_exempi=auto])

if test "x$enable_exempi" != "xno" ; then
   PKG_CHECK_MODULES(EXEMPI,
                     [exempi-2.0 >= $EXEMPI_REQUIRED],
                     [have_exempi=yes],
                     [have_exempi=no])

   TRACKER_WRITEBACK_CFLAGS="$TRACKER_WRITEBACK_CFLAGS $EXEMPI_CFLAGS"
   TRACKER_WRITEBACK_LIBS="$TRACKER_WRITEBACK_LIBS $EXEMPI_LIBS"

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $EXEMPI_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $EXEMPI_LIBS"

   if test "x$have_exempi" = "xyes"; then
      AC_DEFINE(HAVE_EXEMPI, [], [Define if we have exempi])
   fi
else
   have_exempi="no  (disabled)"
fi

if test "x$enable_exempi" = "xyes"; then
   if test "x$have_exempi" != "xyes"; then
      AC_MSG_ERROR([Could not find exempi >= $EXEMPI_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_EXEMPI, test "x$have_exempi" = "xyes")

####################################################################
# Check for tracker-miner-fs: meegotouch, for MeeGo desktop files
####################################################################

AC_ARG_ENABLE(meegotouch,
              AS_HELP_STRING([--enable-meegotouch],
                             [enable libmeegotouch application extraction [[default=no]]]),
              [enable_meegotouch=$enableval],
              [enable_meegotouch=no])

if test "x$enable_meegotouch" != "xno"; then
   PKG_CHECK_MODULES(MEEGOTOUCH,
                     [meegotouch >= $MEEGOTOUCH_REQUIRED],
                     [have_meegotouch=yes],
                     [have_meegotouch=no])

   LIBTRACKER_COMMON_CFLAGS="$LIBTRACKER_COMMON_CFLAGS $MEEGOTOUCH_CFLAGS"
   LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS $MEEGOTOUCH_LIBS"

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $MEEGOTOUCH_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $MEEGOTOUCH_LIBS"

   TRACKER_MINER_FS_CFLAGS="$TRACKER_MINER_FS_CFLAGS $MEEGOTOUCH_CFLAGS"
   TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS $MEEGOTOUCH_LIBS"

   if test "x$have_meegotouch" = "xyes"; then
      AC_DEFINE(HAVE_MEEGOTOUCH, [], [Define if we have meegotouch])
   fi
else
   have_meegotouch="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_MEEGOTOUCH, test "x$have_meegotouch" = "xyes")

##################################################################
# Check for tracker-miner-fs
##################################################################

have_tracker_miner_fs="yes"

AC_ARG_ENABLE(miner_fs,
              AS_HELP_STRING([--enable-miner-fs],
                             [enable File System miner [[default=yes]]]),
              [enable_miner_fs=$enableval],
              [enable_miner_fs=yes])

enable_tracker_miner_fs=$enable_miner_fs

if test "x$enable_tracker_miner_fs" = "xyes"; then
   if test "x$have_tracker_miner_fs" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-miner-fs dependencies ($TRACKER_MINER_FS_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_fs" = "xno"; then
      have_tracker_miner_fs="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_MINER_FS, test "x$have_tracker_miner_fs" = "xyes")

##################################################################
# Check for tracker-extract
##################################################################

have_tracker_extract="yes"

AC_ARG_ENABLE(tracker-extract,
              AS_HELP_STRING([--enable-extract],
                             [enable Extractor miner [[default=yes]]]),
              [enable_tracker_extract=$enableval],
              [enable_tracker_extract=yes])

if test "x$enable_tracker_extract" = "xyes"; then
   if test "x$have_tracker_extract" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-extract dependencies ($TRACKER_EXTRACT_REQUIRED).])
   fi
else
   if test "x$enable_tracker_extract" = "xno"; then
      have_tracker_extract="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_EXTRACT, test "x$have_tracker_extract" = "xyes")

##################################################################
# Check for tracker-writeback
##################################################################

have_tracker_writeback="yes"

AC_ARG_ENABLE(tracker-writeback,
              AS_HELP_STRING([--enable-tracker-writeback],
                             [enable writeback [[default=yes]]]),
              [enable_tracker_writeback=$enableval],
              [enable_tracker_writeback=yes])

if test "x$enable_tracker_writeback" = "xyes"; then
   if test "x$have_tracker_writeback" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-writeback dependencies ($TRACKER_WRITEBACK_REQUIRED).])
   fi
else
   if test "x$enable_tracker_writeback" = "xno"; then
      have_tracker_writeback="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_WRITEBACK, test "x$have_tracker_writeback" = "xyes")

##################################################################
# Check for tracker-miner-apps
##################################################################

have_tracker_miner_apps="yes"

AC_ARG_ENABLE(miner_apps,
              AS_HELP_STRING([--enable-miner-apps],
                             [enable Application miner [[default=yes]]]),
              [enable_miner_apps=$enableval],
              [enable_miner_apps=yes])

enable_tracker_miner_apps=$enable_miner_apps

if test "x$enable_tracker_miner_apps" = "xyes"; then
   if test "x$have_tracker_miner_apps" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-miner-apps dependencies ($TRACKER_MINER_APPS_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_apps" = "xno"; then
      have_tracker_miner_apps="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_MINER_APPS, test "x$have_tracker_miner_apps" = "xyes")

##################################################################
# Check for tracker-miner-user-guides
##################################################################

have_tracker_miner_user_guides="yes"

AC_ARG_ENABLE(miner_user_guides,
              AS_HELP_STRING([--enable-miner-user-guides],
                             [enable User guides miner [[default=auto]]]),
              [enable_miner_user_guides=$enableval],
              [enable_miner_user_guides=auto])

enable_tracker_miner_user_guides=$enable_miner_user_guides

if test "x$enable_tracker_miner_user_guides" = "xyes"; then
   if test "x$have_tracker_miner_user_guides" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-miner-user-guides dependencies ($TRACKER_MINER_USER_GUIDES_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_user_guides" = "xno"; then
      have_tracker_miner_user_guides="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_MINER_USER_GUIDES, test "x$have_tracker_miner_user_guides" = "xyes")

##################################################################
# Check for tracker-miner-rss
##################################################################

AC_ARG_ENABLE([miner_rss],
              AS_HELP_STRING([--enable-miner-rss],
                             [enable RSS data miner [[default=auto]]]),
              [enable_miner_rss=$enableval],
              [enable_miner_rss=auto])

enable_tracker_miner_rss=$enable_miner_rss

if test "x$enable_tracker_miner_rss" = "xyes" ; then
   if test "x$have_tracker_miner_rss" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-miner-rss dependencies ($TRACKER_MINER_RSS_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_rss" = "xno" ; then
      have_tracker_miner_rss="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_MINER_RSS, test "x$have_tracker_miner_rss" = "xyes")

####################################################################
# Check for tracker-miner-evolution
####################################################################

tracker_miner_evolution_install_dir="/dev/null"

AC_ARG_ENABLE([miner_evolution],
              AS_HELP_STRING([--enable-miner-evolution],
                             [enable Evolution email data miner [[default=auto]]]),
              [enable_miner_evolution=$enableval],
              [enable_miner_evolution=auto])

enable_tracker_miner_evolution=$enable_miner_evolution

if test "x$enable_tracker_miner_evolution" = "xyes"; then
   if test "x$have_tracker_miner_evolution" != "xyes"; then
      AC_MSG_ERROR([Could not find Tracker Evolution plugin ($TRACKER_MINER_EVOLUTION_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_evolution" = "xno"; then
      have_tracker_miner_evolution="no  (disabled)"
   fi
fi

if test "x$have_tracker_miner_evolution" = "xyes"; then
   AC_ARG_WITH([evolution_plugin_dir],
               AS_HELP_STRING([--with-evolution-plugin-dir],
                              [path to Evolution plugin directory])),
               [tracker_miner_evolution_install_dir=$withval],
               [tracker_miner_evolution_install_dir=`$PKG_CONFIG $evolution_plugin_name --variable=plugindir`],

   if test -z $tracker_miner_evolution_install_dir; then
      AC_MSG_ERROR([Could not find evolution plugin dir, consider using --with-evolution-plugin-dir.])
   fi
fi

TRACKER_MINER_EVOLUTION_INSTALL_DIR=$tracker_miner_evolution_install_dir
AC_SUBST(TRACKER_MINER_EVOLUTION_INSTALL_DIR)

AM_CONDITIONAL(HAVE_TRACKER_MINER_EVOLUTION, test "x$have_tracker_miner_evolution" = "xyes")

####################################################################
# Check for tracker-miner-thunderbird
####################################################################

tracker_miner_thunderbird_install_dir="/dev/null"

AC_ARG_ENABLE([miner_thunderbird],
              AS_HELP_STRING([--enable-miner-thunderbird],
                             [enable Thunderbird email data miner [[default=auto]]]),
              [enable_miner_thunderbird=$enableval],
              [enable_miner_thunderbird=auto])

enable_tracker_miner_thunderbird=$enable_miner_thunderbird

if test "x$enable_tracker_miner_thunderbird" = "xyes"; then
   if test "x$have_tracker_miner_thunderbird" != "xyes"; then
      AC_MSG_ERROR([Could not find Thunderbird ($TRACKER_MINER_THUNDERBIRD_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_thunderbird" = "xno"; then
      have_tracker_miner_thunderbird="no  (disabled)"
   fi
fi

if test "x$have_tracker_miner_thunderbird" = "xyes"; then
   # This is a symlinked to $prefix/share/xul-ext
   # This was ${prefix}/lib/thunderbird-${thunderbird_version}/extensions
   AC_ARG_WITH([thunderbird_plugin_dir],
               AS_HELP_STRING([--with-thunderbird-plugin-dir],
                              [path to Thunderbird plugin directory]),
               [tracker_miner_thunderbird_install_dir=$withval],
               [tracker_miner_thunderbird_install_dir=${prefix}/lib/thunderbird-addons/extensions])

   if test -z $tracker_miner_thunderbird_install_dir; then
      AC_MSG_ERROR([Could not find thunderbird plugin dir, consider using --with-thunderbird-plugin-dir.])
   fi
fi

TRACKER_MINER_THUNDERBIRD_INSTALL_DIR=$tracker_miner_thunderbird_install_dir
AC_SUBST(TRACKER_MINER_THUNDERBIRD_INSTALL_DIR)

AM_CONDITIONAL(HAVE_TRACKER_MINER_THUNDERBIRD, test "x$have_tracker_miner_thunderbird" = "xyes")

####################################################################
# Check for tracker-miner-firefox
####################################################################

tracker_miner_firefox_install_dir="/dev/null"

AC_ARG_ENABLE([miner_firefox],
              AS_HELP_STRING([--enable-miner-firefox],
                             [enable Firefox email data miner [[default=auto]]]),
              [enable_miner_firefox=$enableval],
              [enable_miner_firefox=auto])

enable_tracker_miner_firefox=$enable_miner_firefox

if test "x$enable_tracker_miner_firefox" = "xyes"; then
   if test "x$have_tracker_miner_firefox" != "xyes"; then
      AC_MSG_ERROR([Could not find Firefox ($TRACKER_MINER_FIREFOX_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_firefox" = "xno"; then
      have_tracker_miner_firefox="no  (disabled)"
   fi
fi

if test "x$have_tracker_miner_firefox" = "xyes"; then
   # This is a symlinked to $prefix/share/xul-ext
   # This was ${prefix}/lib/firefox-${firefox_version}/extensions
   AC_ARG_WITH([firefox_plugin_dir],
               AS_HELP_STRING([--with-firefox-plugin-dir],
                              [path to Firefox plugin directory]),
               [tracker_miner_firefox_install_dir=$withval],
               [tracker_miner_firefox_install_dir=${prefix}/lib/firefox-addons/extensions])

   if test -z $tracker_miner_firefox_install_dir; then
      AC_MSG_ERROR([Could not find firefox plugin dir, consider using --with-firefox-plugin-dir.])
   fi
fi

TRACKER_MINER_FIREFOX_INSTALL_DIR=$tracker_miner_firefox_install_dir
AC_SUBST(TRACKER_MINER_FIREFOX_INSTALL_DIR)

AM_CONDITIONAL(HAVE_TRACKER_MINER_FIREFOX, test "x$have_tracker_miner_firefox" = "xyes")

####################################################################
# Check for nautilus extension
####################################################################

AC_ARG_ENABLE([nautilus-extension],
              AS_HELP_STRING([--enable-nautilus-extension],
                             [enable tagging in nautilus with Tracker [[default=auto]]]),
              [enable_nautilus_extension=$enableval],
              [enable_nautilus_extension=auto])

enable_tracker_nautilus_extension=$enable_nautilus_extension

if test "x$enable_tracker_nautilus_extension" = "xyes"; then
   if test "x$have_tracker_nautilus_extension" != "xyes"; then
      AC_MSG_ERROR([Could not find Tracker Nautilus extension dependencies ($TRACKER_NAUTILUS_EXTENSION_REQUIRED).])
   fi
else
   if test "x$enable_tracker_nautilus_extension" = "xno"; then
     have_tracker_nautilus_extension="no  (disabled)"
   fi
fi

if test "x$have_tracker_nautilus_extension" = "xyes"; then
   AC_ARG_WITH([nautilus-extensions-dir],
               AS_HELP_STRING([--with-nautilus-extensions-dir],
                              [path to Nautilus extensions directory]),
               [tracker_nautilus_extension_install_dir=$withval],
               [tracker_nautilus_extension_install_dir=`$PKG_CONFIG --variable=extensiondir libnautilus-extension`])

   TRACKER_NAUTILUS_EXTENSION_INSTALL_DIR="$tracker_nautilus_extension_install_dir"
else
   TRACKER_NAUTILUS_EXTENSION_INSTALL_DIR="/dev/null"
fi

AC_SUBST([TRACKER_NAUTILUS_EXTENSION_INSTALL_DIR])

AM_CONDITIONAL(HAVE_TRACKER_NAUTILUS_EXTENSION, test "$have_tracker_nautilus_extension" = "yes")

####################################################################
# Check for tracker-writeback: Taglib for audio writeback support
####################################################################

AC_ARG_ENABLE(taglib,
              AS_HELP_STRING([--enable-taglib],
                             [enable writeback for audio files [[default=auto]]]),
              [enable_taglib=$enableval],
              [enable_taglib=auto])

if test "x$enable_taglib" != "xno" ; then
   PKG_CHECK_MODULES(TAGLIB,
                     [taglib_c >= $TAGLIB_REQUIRED],
                     [have_taglib=yes],
                     [have_taglib=no])

   TRACKER_WRITEBACK_CFLAGS="$TRACKER_WRITEBACK_CFLAGS $TAGLIB_CFLAGS"
   TRACKER_WRITEBACK_LIBS="$TRACKER_WRITEBACK_LIBS $TAGLIB_LIBS"
else
   have_taglib="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_TAGLIB, test "x$have_taglib" = "xyes")

##################################################################
# Check for tracker-needle
##################################################################

AC_ARG_ENABLE([tracker-needle],
              AS_HELP_STRING([--enable-tracker-needle],
                             [enable GTK+ UI to search for content [[default=auto]]]),
              [enable_tracker_needle=$enableval],
              [enable_tracker_needle=auto])

if test "x$enable_tracker_needle" = "xyes" ; then
   if test "x$have_tracker_needle" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-needle dependencies ($TRACKER_NEEDLE_REQUIRED).])
   fi
else
   if test "x$enable_tracker_needle" = "xno"; then
      have_tracker_needle="no  (disabled)"
   fi
fi

if test "x$have_tracker_needle" = "xyes"; then
   AC_DEFINE(HAVE_TRACKER_NEEDLE, [1], [Define to 1 if tracker-needle is compiled])
else
   AC_DEFINE(HAVE_TRACKER_NEEDLE, [0], [Define to 0 if tracker-needle is not compiled])
fi

AM_CONDITIONAL(HAVE_TRACKER_NEEDLE, test "$have_tracker_needle" = "yes")

##################################################################
# Check for tracker-preferences
##################################################################

AC_ARG_ENABLE([tracker-preferences],
              AS_HELP_STRING([--enable-tracker-preferences],
                             [enable GTK+ UI to configure Tracker [[default=auto]]]),
              [enable_tracker_preferences=$enableval],
              [enable_tracker_preferences=auto])

# If we don't have miner-fs enabled, the preferences are pointless
# So we override in this case.
if test "x$enable_tracker_miner_fs" = "xyes" ; then
   enable_tracker_miner_fs="no"
fi

if test "x$enable_tracker_preferences" = "xyes" ; then
   if test "x$have_tracker_preferences" != "xyes"; then
      AC_MSG_ERROR([Could not find tracker-preferences dependencies ($TRACKER_PREFERENCES_REQUIRED).])
   fi
else
   if test "x$enable_tracker_preferences" = "xno"; then
      have_tracker_preferences="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_PREFERENCES, test "$have_tracker_preferences" = "yes")

##################################################################
# Check for tracker-resdump
##################################################################

AM_CONDITIONAL(HAVE_TRACKER_RESDUMP, test "$have_tracker_resdump" = "yes")

####################################################################
####################################################################
####################################################################
# This section is for tracker-extract dependencies
####################################################################
####################################################################
####################################################################

####################################################################
# Check for tracker-extract: enca for defect Russian/Cyrillic in MP3s
####################################################################

AC_ARG_ENABLE(enca,
              AS_HELP_STRING([--enable-enca],
                             [enable libenca for Cyrillic language detection in MP3s [[default=auto]]]),
              [enable_enca=$enableval],
              [enable_enca=auto])

if test "x$enable_enca" != "xno" ; then
   PKG_CHECK_MODULES(ENCA, [enca >= 1.9], have_enca=yes, have_enca=no)

   LIBTRACKER_EXTRACT_CFLAGS="$LIBTRACKER_EXTRACT_CFLAGS $ENCA_CFLAGS"
   LIBTRACKER_EXTRACT_LIBS="$LIBTRACKER_EXTRACT_LIBS $ENCA_LIBS"

   if test "x$have_enca" = "xyes"; then
     AC_DEFINE(HAVE_ENCA, [], [Enca language detection aid])
   fi
else
   have_enca="no  (disabled)"
fi

AC_ARG_ENABLE(icu-charset-detection,
              AS_HELP_STRING([--enable-icu-charset-detection],
                             [enable libicu for charset detection in MP3s [[default=auto]]]),
              [enable_icu_charset_detection=$enableval],
              [enable_icu_charset_detection=auto])

if test "x$have_libicu" = "xyes"; then
	if test "x$enable_icu_charset_detection" != "xno"; then
		have_libicu_charset_detection="yes"
		AC_DEFINE(HAVE_LIBICU_CHARSET_DETECTION, [], [ICU language detection aid])
	else
		have_libicu_charset_detection="no"
	fi
fi

if test "x$have_enca" = "xyes" || test "x$have_libicu_charset_detection" = "xyes"; then
   have_charset_detection="yes"
else
   have_charset_detection="no"
fi

AM_CONDITIONAL(HAVE_ENCA, test "$have_enca" = "yes")
AM_CONDITIONAL(HAVE_LIBICU_CHARSET_DETECTION, test "$have_libicu_charset_detection" = "yes")

##################################################################
# Check for tracker-extract: libxml2 for XML/HTML extractor
##################################################################

AC_ARG_ENABLE(libxml2,
              AS_HELP_STRING([--enable-libxml2],
                             [enable extractor for HTML/XML metadata [[default=yes]]]),
              [enable_libxml2=$enableval],
              [enable_libxml2=yes])

if test "x$enable_libxml2" != "xno"; then
   PKG_CHECK_MODULES(LIBXML2,
                     [libxml-2.0 >= $LIBXML2_REQUIRED],
                     [have_libxml2=yes],
                     [have_libxml2=no])
   AC_SUBST(LIBXML2_CFLAGS)
   AC_SUBST(LIBXML2_LIBS)

   if test "x$have_libxml2" = "xyes"; then
      AC_DEFINE(HAVE_LIBXML2, [], [Define if we have libxml2])
   fi
else
   have_libxml2="no  (disabled)"
fi

if test "x$enable_libxml2" = "xyes"; then
   if test "x$have_libxml2" != "xyes"; then
      AC_MSG_ERROR([Could not find libxml2 >= $LIBXML2_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBXML2, test "x$have_libxml2" = "xyes")

####################################################################
# Check for tracker-extract: gstreamer/etc
####################################################################

AC_ARG_ENABLE(generic-media-extractor,
              AS_HELP_STRING([--enable-generic-media-extractor=ARG],
                             [enables one of the (gstreamer, libav, external, auto) generic media extractor backends [[default=auto]]]),
              [enable_generic_media_extractor=$enableval],
              [enable_generic_media_extractor=auto])

PKG_CHECK_MODULES(GSTREAMER,
                  [gstreamer-1.0 >= $GSTREAMER_REQUIRED
                   gstreamer-tag-1.0 >= $GSTREAMER_REQUIRED],
                  [have_libgstreamer=yes],
                  [have_libgstreamer=no])

AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(AVFORMAT,
                  [libavformat >= 0.8.4],
                  [have_libavformat=yes],
                  [have_libavformat=no])

AC_SUBST(AVFORMAT_CFLAGS)
AC_SUBST(AVFORMAT_LIBS)

PKG_CHECK_MODULES(AVCODEC,
                  [libavcodec >= 0.8.4],
                  [have_libavcodec=yes],
                  [have_libavcodec=no])

AC_SUBST(AVCODEC_CFLAGS)
AC_SUBST(AVCODEC_LIBS)

PKG_CHECK_MODULES(AVUTIL,
                  [libavutil >= 0.8.4],
                  [have_libavutil=yes],
                  [have_libavutil=no])

AC_SUBST(AVUTIL_CFLAGS)
AC_SUBST(AVUTIL_LIBS)

if test "x$enable_generic_media_extractor" = "xauto"; then
   if test "$have_libgstreamer" = "yes"; then
      have_generic_media_handler="yes"
      have_generic_media_handler_app="GStreamer"
   elif test "$have_libav" = "yes"; then
      have_generic_media_handler_app="libav"
      have_generic_media_handler="yes"
   else
      have_generic_media_handler="?"
      have_generic_media_handler_app="An external generic_media player will be called"
   fi
elif test "x$enable_generic_media_extractor" = "xgstreamer"; then
   if test "$have_libgstreamer" = "yes"; then
      have_generic_media_handler_app="GStreamer"
      have_generic_media_handler="yes"
   else
      AC_MSG_ERROR([Could not find GStreamer >= $GSTREAMER_REQUIRED])
   fi
elif test "x$enable_generic_media_extractor" = "xlibav"; then
   if test "$have_libavformat" = "yes" && test "$have_libavcodec" = "yes" && test "$have_libavutil" = "yes"; then
      have_generic_media_handler_app="libav"
      have_generic_media_handler="yes"
   else
      AC_MSG_ERROR([Could not find libav])
   fi
else
   have_generic_media_handler="?"
   have_generic_media_handler_app="An external generic media player will be called"
fi

if test "$have_generic_media_handler_app" = "GStreamer"; then
   AC_DEFINE(HAVE_GSTREAMER, [], [Define if we have GStreamer])
   AM_CONDITIONAL(HAVE_GSTREAMER, true)
   AM_CONDITIONAL(HAVE_LIBAV, false)
elif test "$have_generic_media_handler_app" = "libav"; then
   AC_DEFINE(HAVE_LIBAV, [], [Define if we have libav])
   AM_CONDITIONAL(HAVE_LIBAV, true)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
else
   AM_CONDITIONAL(HAVE_LIBAV, false)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
fi

###########################################################################
# Check for gstreamer backend: tagreadbin/decodebin2/discoverer/gupnp-dlna
###########################################################################

AC_ARG_WITH([gstreamer-backend],
            AS_HELP_STRING([--with-gstreamer-backend],
                           [enables one of the (gupnp-dlna, discoverer) GStreamer backends [[default=discoverer]]]),
            [with_gstreamer_backend=$withval],
            [with_gstreamer_backend=discoverer])

if test "$have_generic_media_handler_app" != "GStreamer"; then
   AC_MSG_WARN([No need to specify a GStreamer backend if not using GStreamer])
   gstreamer_backend="N/A"
else
   case "x$with_gstreamer_backend" in
        "xdiscoverer")
           PKG_CHECK_MODULES(GSTREAMER_PBUTILS,
                             [gstreamer-pbutils-1.0 >= $GSTREAMER_REQUIRED],
                             [have_gstreamer_pbutils=yes],
                             [have_gstreamer_pbutils=no])
           AC_SUBST(GSTREAMER_PBUTILS_CFLAGS)
           AC_SUBST(GSTREAMER_PBUTILS_LIBS)

           if test "x$have_gstreamer_pbutils" != "xyes"; then
              AC_MSG_ERROR([Could not find GStreamer pbutils])
           fi

           gstreamer_backend="Discoverer"
           AC_DEFINE(GSTREAMER_BACKEND_DISCOVERER, [], [Define that GStreamer Discoverer should be used])
        ;;
        "xgupnp-dlna")
           PKG_CHECK_MODULES(GUPNP_DLNA,
                             [gupnp-dlna-2.0 >= $GUPNP_DLNA_REQUIRED gupnp-dlna-gst-2.0 >= $GUPNP_DLNA_REQUIRED],
                             [have_gupnp_dlna=yes],
                             [have_gupnp_dlna=no])
           AC_SUBST(GUPNP_DLNA_CFLAGS)
           AC_SUBST(GUPNP_DLNA_LIBS)

           if test "x$have_gupnp_dlna" = "xyes"; then
              gstreamer_backend="GUPnP-DLNA"
              AC_DEFINE(GSTREAMER_BACKEND_GUPNP_DLNA, [], [Define that GUPnP-DLNA should be used])
           else
              AC_MSG_ERROR([Could not find GUPnP-DLNA >= $GUPNP_DLNA_REQUIRED])
           fi
        ;;
        # Invalid option value
        *) AC_MSG_ERROR([Wrong value for --with-gstreamer-backend: $with_gstreamer_backend]) ;;
   esac
fi

AM_CONDITIONAL(HAVE_GUPNP_DLNA,        test "x$gstreamer_backend" = "xGUPnP-DLNA")
AM_CONDITIONAL(HAVE_GSTREAMER_PBUTILS, test "x$gstreamer_backend" = "xDiscoverer")

####################################################################
# Check for tracker-extract: support .ps.gz files
####################################################################

AC_ARG_ENABLE([unzip_psgz_files],
              AS_HELP_STRING([--enable-unzip-ps-gz-files],
                             [enable extractor for PS.GZ metadata [[default=auto]]]),
              [enable_unzip_psgz_files=$enableval],
              [enable_unzip_psgz_files=auto])

if test x$enable_unzip_psgz_files != "xno"; then
  AC_DEFINE(USING_UNZIPPSFILES, 1, [Unzip ps.gz files])
fi

##################################################################
# Check for tracker-extract: poppler for PDF
##################################################################

AC_ARG_ENABLE(poppler,
              AS_HELP_STRING([--enable-poppler],
                             [enable extractor for PDF metadata [[default=auto]]]),
              [enable_poppler=$enableval],
              [enable_poppler=auto])

if test "x$enable_poppler" != "xno" ; then
   PKG_CHECK_MODULES(POPPLER,
                     [poppler-glib >= $POPPLER_REQUIRED],
                     [have_poppler=yes],
                     [have_poppler=no])

   AC_SUBST(POPPLER_CFLAGS)
   AC_SUBST(POPPLER_LIBS)

   if test "x$have_poppler" = "xyes"; then
      AC_DEFINE(HAVE_POPPLER, [], [Define if we have poppler-glib])
   fi
else
   have_poppler="no  (disabled)"
fi

if test "x$enable_poppler" = "xyes"; then
   if test "x$have_poppler" != "xyes"; then
      AC_MSG_ERROR([Could not find poppler-glib >= $POPPLER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_POPPLER, test "x$have_poppler" = "xyes")

##################################################################
# Check for tracker-extract: libgxps for XPS
##################################################################

AC_ARG_ENABLE(libgxps,
              AS_HELP_STRING([--enable-libgxps],
                             [enable extractor for XPS metadata [[default=auto]]]),
              [enable_libgxps=$enableval],
              [enable_libgxps=auto])

if test "x$enable_libgxps" != "xno" ; then
   PKG_CHECK_MODULES(LIBGXPS,
                     [libgxps],
                     [have_libgxps=yes],
                     [have_libgxps=no])

   AC_SUBST(LIBGXPS_CFLAGS)
   AC_SUBST(LIBGXPS_LIBS)

   if test "x$have_libgxps" = "xyes"; then
      AC_DEFINE(HAVE_LIBGXPS, [], [Define if we have libgxps])
   fi
else
   have_libgxps="no  (disabled)"
fi

if test "x$enable_libgxps" = "xyes"; then
   if test "x$have_libgxps" != "xyes"; then
      AC_MSG_ERROR([Could not find libgxps.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBGXPS, test "x$have_libgxps" = "xyes")

##################################################################
# Check for tracker-extract: libgsf
##################################################################

AC_ARG_ENABLE(libgsf,
              AS_HELP_STRING([--enable-libgsf],
                             [enable extractor for GSF metadata (MSOffice, LibreOffice, EPub) [[default=auto]]]),
              [enable_libgsf=$enableval],
              [enable_libgsf=auto])

if test "x$enable_libgsf" != "xno" ; then
   PKG_CHECK_MODULES(LIBGSF,
                     [libgsf-1 >= $LIBGSF_REQUIRED],
		     [have_libgsf=yes],
		     [have_libgsf=no])

   AC_SUBST(LIBGSF_CFLAGS)
   AC_SUBST(LIBGSF_LIBS)

   if test "x$have_libgsf" = "xyes"; then
      AC_DEFINE(HAVE_LIBGSF, [], [Define if we have libgsf])
   fi
else
   have_libgsf="no  (disabled)"
fi

if test "x$enable_libgsf" = "xyes"; then
   if test "x$have_libgsf" != "xyes"; then
      AC_MSG_ERROR([Could not find libgsf >= $LIBGSF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBGSF, test "x$have_libgsf" = "xyes")

##################################################################
# Check for tracker-extract: libosinfo
##################################################################

AC_ARG_ENABLE(libosinfo,
              AS_HELP_STRING([--enable-libosinfo],
                             [enable extractor for ISO metadata [[default=auto]]]),
              [enable_libosinfo=$enableval],
              [enable_libosinfo=auto])

if test "x$enable_libosinfo" != "xno" ; then
   PKG_CHECK_MODULES(LIBOSINFO,
                     [libosinfo-1.0 >= $LIBOSINFO_REQUIRED],
		     [have_libosinfo=yes],
		     [have_libosinfo=no])

   AC_SUBST(LIBOSINFO_CFLAGS)
   AC_SUBST(LIBOSINFO_LIBS)

   if test "x$have_libosinfo" = "xyes"; then
      AC_DEFINE(HAVE_LIBOSINFO, [], [Define if we have libosinfo])
   fi
else
   have_libosinfo="no  (disabled)"
fi

if test "x$enable_libosinfo" = "xyes"; then
   if test "x$have_libosinfo" != "xyes"; then
      AC_MSG_ERROR([Could not find libosinfo >= $LIBOSINFO_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBOSINFO, test "x$have_libosinfo" = "xyes")

##################################################################
# Check for tracker-extract: libgif
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LDFLAGS="$LDFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LDFLAGS=""
LIBS=""

AC_ARG_ENABLE(libgif,
              AS_HELP_STRING([--enable-libgif],
                             [enable extractor for GIF metadata [[default=auto]]]),
              [enable_libgif=$enableval],
              [enable_libgif=auto])

if test "x$enable_libgif" != "xno" ; then
   AC_CHECK_HEADER(gif_lib.h,
   AC_CHECK_LIB(gif, DGifOpen))

   have_libgif=${ac_cv_lib_gif_DGifOpen:-no}

   LIBGIF_CFLAGS="$CFLAGS"
   LIBGIF_LIBS="$LIBS"

   AC_SUBST(LIBGIF_CFLAGS)
   AC_SUBST(LIBGIF_LIBS)

   if test "x$have_libgif" = "xyes"; then
      AC_DEFINE(HAVE_LIBGIF, [], [Define if we have libgif])
   fi
else
   have_libgif="no  (disabled)"
fi

if test "x$enable_libgif" = "xyes"; then
   if test "x$have_libgif" != "xyes"; then
      AC_MSG_ERROR([Could not find libgif >= $LIBGIF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBGIF, test "x$have_libgif" = "xyes")

CFLAGS="$OLD_CFLAGS"
LDFLAGS="$OLD_LDFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: libjpeg
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LDFLAGS="$LDFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LDFLAGS=""
LIBS=""

AC_ARG_ENABLE(libjpeg,
              AS_HELP_STRING([--enable-libjpeg],
                             [enable extractor for JPEG metadata [[default=auto]]]),
              [enable_libjpeg=$enableval],
              [enable_libjpeg=auto])

if test "x$enable_libjpeg" != "xno" ; then
   AC_CHECK_HEADER(jpeglib.h,
   AC_CHECK_LIB(jpeg, jpeg_CreateCompress))

   have_libjpeg=${ac_cv_lib_jpeg_jpeg_CreateCompress:-no}

   LIBJPEG_CFLAGS="$CFLAGS"
   LIBJPEG_LIBS="$LIBS"

   AC_SUBST(LIBJPEG_CFLAGS)
   AC_SUBST(LIBJPEG_LIBS)

   if test "x$have_libjpeg" = "xyes"; then
      AC_DEFINE(HAVE_LIBJPEG, [], [Define if we have libjpeg])
   fi
else
   have_libjpeg="no  (disabled)"
fi

if test "x$enable_libjpeg" = "xyes"; then
   if test "x$have_libjpeg" != "xyes"; then
      AC_MSG_ERROR([Could not find libjpeg >= $LIBJPEG_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBJPEG, test "x$have_libjpeg" = "xyes")

CFLAGS="$OLD_CFLAGS"
LDFLAGS="$OLD_LDFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: libtiff
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LDFLAGS="$LDFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LDFLAGS=""
LIBS=""

AC_ARG_ENABLE(libtiff,
              AS_HELP_STRING([--enable-libtiff],
                             [enable extractor for TIFF metadata [[default=auto]]]),
              [enable_libtiff=$enableval],
              [enable_libtiff=auto])

if test "x$enable_libtiff" != "xno" ; then
   AC_CHECK_HEADER(tiff.h,
                   AC_CHECK_LIB(tiff, TIFFOpen))

   have_libtiff=${ac_cv_lib_tiff_TIFFOpen:-no}

   LIBTIFF_CFLAGS="$CFLAGS"
   LIBTIFF_LIBS="$LIBS"

   AC_SUBST(LIBTIFF_CFLAGS)
   AC_SUBST(LIBTIFF_LIBS)

   if test "x$have_libtiff" = "xyes"; then
      AC_DEFINE(HAVE_LIBTIFF, [], [Define if we have libtiff])
   fi
else
   have_libtiff="no  (disabled)"
fi

if test "x$enable_libtiff" = "xyes"; then
   if test "x$have_libtiff" != "xyes"; then
      AC_MSG_ERROR([Could not find libtiff >= $LIBTIFF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBTIFF, test "x$have_libtiff" = "xyes")

CFLAGS="$OLD_CFLAGS"
LDFLAGS="$OLD_LDFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: libpng
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LDFLAGS="$LDFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LDFLAGS=""
LIBS=""

AC_ARG_ENABLE(libpng,
              AS_HELP_STRING([--enable-libpng],
                             [enable extractor for PNG metadata [[default=auto]]]),
              [enable_libpng=$enableval],
              [enable_libpng=auto])

if test "x$enable_libpng" != "xno" ; then
   AC_CHECK_HEADER(png.h,
                   AC_CHECK_LIB(png, png_create_read_struct))

   have_libpng=${ac_cv_lib_png_png_create_read_struct:-no}

   LIBPNG_CFLAGS="$CFLAGS"
   LIBPNG_LIBS="$LIBS"

   AC_SUBST(LIBPNG_CFLAGS)
   AC_SUBST(LIBPNG_LIBS)

   if test "x$have_libpng" = "xyes"; then
      AC_DEFINE(HAVE_LIBPNG, [], [Define if we have libpng])
   fi
else
   have_libpng="no  (disabled)"
fi

if test "x$enable_libpng" = "xyes"; then
   if test "x$have_libpng" != "xyes"; then
      AC_MSG_ERROR([Could not find libpng >= $LIBPNG_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBPNG, test "x$have_libpng" = "xyes")

CFLAGS="$OLD_CFLAGS"
LDFLAGS="$OLD_LDFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: vorbis
##################################################################

AC_ARG_ENABLE(libvorbis,
              AS_HELP_STRING([--enable-libvorbis],
                             [enable extractor for OGG metadata (Vorbis) [[default=no]]]),
              [enable_libvorbis=$enableval],
              [enable_libvorbis=no])

if test "x$enable_libvorbis" != "xno" ; then
   PKG_CHECK_MODULES(LIBVORBIS,
                     [vorbisfile >= $LIBVORBIS_REQUIRED],
                     [have_libvorbis=yes],
                     [have_libvorbis=no])

   AC_SUBST(LIBVORBIS_CFLAGS)
   AC_SUBST(LIBVORBIS_LIBS)

   if test "x$have_libvorbis" = "xyes"; then
      AC_DEFINE(HAVE_LIBVORBIS, [], [Define if we have libvorbis])
   fi
else
   have_libvorbis="no  (disabled)"
fi

if test "x$enable_libvorbis" = "xyes"; then
   if test "x$have_libvorbis" != "xyes"; then
      AC_MSG_ERROR([Could not find libvorbis >= $LIBVORBIS_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBVORBIS, test "x$have_libvorbis" = "xyes")

####################################################################
# Check for tracker-extract: flac
####################################################################

AC_ARG_ENABLE(libflac,
              AS_HELP_STRING([--enable-libflac],
                             [enable extractor for FLAC metadata [[default=no]]]),
              [enable_libflac=$enableval],
              [enable_libflac=no])

if test "x$enable_libflac" != "xno" ; then
   PKG_CHECK_MODULES(LIBFLAC,
                     [flac >= $LIBFLAC_REQUIRED],
                     [have_libflac=yes],
                     [have_libflac=no])
   AC_SUBST(LIBFLAC_CFLAGS)
   AC_SUBST(LIBFLAC_LIBS)

   if test "x$have_libflac" = "xyes"; then
      AC_DEFINE(HAVE_LIBFLAC, [], [Define if we have libflac])
   fi
else
   have_libflac="no  (disabled)"
fi

if test "x$enable_libflac" = "xyes"; then
   if test "x$have_libflac" != "xyes"; then
      AC_MSG_ERROR([Could not find libflac >= $LIBFLAC_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBFLAC, test "x$have_libflac" = "xyes")

####################################################################
# Check for tracker-extract-gstreamer: libcue
####################################################################

AC_ARG_ENABLE(libcue,
              AS_HELP_STRING([--enable-libcue],
                             [enable external cuesheet parsing  [[default=auto]]]),
              [enable_libcue=$enableval],
              [enable_libcue=auto])

if test "x$enable_libcue" != "xno" ; then
   PKG_CHECK_MODULES(LIBCUE,
                     [libcue],
                     [have_libcue=yes],
                     [have_libcue=no])

   AC_SUBST(LIBCUE_CFLAGS)
   AC_SUBST(LIBCUE_LIBS)

   if test "x$have_libcue" = "xyes"; then
      AC_DEFINE(HAVE_LIBCUE, [], [Define if we have libcue])

      PKG_CHECK_EXISTS([libcue >= 2.0.0], have_libcue2=yes, have_libcue2=no)
      if test "x$have_libcue2" = "xyes"; then
        AC_DEFINE(HAVE_LIBCUE2, [], [Define if we have libcue 2 or up])
      fi
   fi
else
   have_libcue="no  (disabled)"
fi

if test "x$enable_libcue" = "xyes"; then
   if test "x$have_libcue" != "xyes"; then
      AC_MSG_ERROR([Could not find libcue.])
   fi
fi

####################################################################
# Check for tracker-extract: AbiWord
####################################################################

AC_ARG_ENABLE(abiword,
              AS_HELP_STRING([--enable-abiword],
                             [enable extractor for ABW metadata (AbiWord) [[default=auto]]]),
              [enable_abiword=$enableval],
              [enable_abiword=auto])

if test "x$enable_abiword" != "xno" ; then
   AC_SUBST(ABIWORD_CFLAGS)
   AC_SUBST(ABIWORD_LIBS)
   AC_DEFINE(HAVE_ABIWORD, [], [Define if we have AbiWord extractor])
   have_abiword=yes
else
   have_abiword="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_ABIWORD, test "x$have_abiword" = "xyes")

####################################################################
# Check for tracker-extract: DVI
####################################################################

AC_ARG_ENABLE(dvi,
              AS_HELP_STRING([--enable-dvi],
                             [enable extractor for DVI metadata [[default=auto]]]),
              [enable_dvi=$enableval],
              [enable_dvi=auto])

if test "x$enable_dvi" != "xno"; then
   AC_SUBST(DVI_CFLAGS)
   AC_SUBST(DVI_LIBS)
   AC_DEFINE(HAVE_DVI, [], [Define if we have DVI extractor])
   have_dvi=yes
else
   have_dvi="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_DVI, test "x$have_dvi" = "xyes")

####################################################################
# Check for tracker-extract: mp3
####################################################################

AC_ARG_ENABLE(mp3,
              AS_HELP_STRING([--enable-mp3],
                             [enable extractor for MP3 metadata [[default=auto]]]),
              [enable_mp3=$enableval],
              [enable_mp3=auto])

if test "x$enable_mp3" != "xno"; then
   AC_SUBST(MP3_CFLAGS)
   AC_SUBST(MP3_LIBS)
   AC_DEFINE(HAVE_MP3, [], [Define if we have MP3 extractor])
   have_mp3=yes
else
   have_mp3="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_MP3, test "x$have_mp3" = "xyes")

####################################################################
# Check for tracker-extract: ps
####################################################################

AC_ARG_ENABLE(ps,
              AS_HELP_STRING([--enable-ps],
                             [enable extractor for PS metadata [[default=auto]]]),
              [enable_ps=$enableval],
              [enable_ps=auto])

if test "x$enable_ps" != "xno"; then
   AC_SUBST(PS_CFLAGS)
   AC_SUBST(PS_LIBS)
   AC_DEFINE(HAVE_PS, [], [Define if we have PS extractor])
   have_ps=yes
else
   have_ps="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_PS, test "x$have_ps" = "xyes")

####################################################################
# Check for tracker-extract: text
####################################################################

AC_ARG_ENABLE(text,
              AS_HELP_STRING([--enable-text],
                             [enable extractor for TXT metadata [[default=auto]]]),
              [enable_text=$enableval],
              [enable_text=auto])

if test "x$enable_text" != "xno"; then
   AC_SUBST(TEXT_CFLAGS)
   AC_SUBST(TEXT_LIBS)
   AC_DEFINE(HAVE_TEXT, [], [Define if we have text extractor])
   have_text=yes
else
   have_text="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_TEXT, test "x$have_text" = "xyes")

####################################################################
# Check for tracker-extract: icon
####################################################################

AC_ARG_ENABLE(icon,
              AS_HELP_STRING([--enable-icon],
                             [enable extractor for ICO metadata [[default=auto]]]),
              [enable_icon=$enableval],
              [enable_icon=auto])

if test "x$enable_icon" != "xno"; then
   AC_SUBST(ICON_CFLAGS)
   AC_SUBST(ICON_LIBS)
   AC_DEFINE(HAVE_ICON, [], [Define if we have ICON extractor])
   have_icon=yes
else
   have_icon="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_ICON, test "x$have_icon" = "xyes")

####################################################################
# Check for tracker-extract, tracker-writeback: totem-pl-parser
####################################################################

AC_ARG_ENABLE(playlist,
              AS_HELP_STRING([--enable-playlist],
                             [enable extractor for Playlist metadata [[default=auto]]]),
              [enable_playlist=$enableval],
              [enable_playlist=auto])

if test "x$enable_playlist" != "xno" ; then
   PKG_CHECK_MODULES(TOTEM_PL_PARSER,
                     [totem-plparser
                      glib-2.0     >= $GLIB_REQUIRED
                      gobject-2.0  >= $GLIB_REQUIRED],
                     [have_playlist=yes],
                     [have_playlist=no])

   TRACKER_WRITEBACK_CFLAGS="$TRACKER_WRITEBACK_CFLAGS $TOTEM_PL_PARSER_CFLAGS"
   TRACKER_WRITEBACK_LIBS="$TRACKER_WRITEBACK_LIBS $TOTEM_PL_PARSER_LIBS"

   AC_SUBST(TOTEM_PL_PARSER_CFLAGS)
   AC_SUBST(TOTEM_PL_PARSER_LIBS)

   if test "x$have_playlist" = "xyes"; then
      AC_DEFINE(HAVE_TOTEM_PL_PARSER, [], [Define if we have Totem PL parser])
   fi
else
   have_playlist="no  (disabled)"
fi

if test "x$enable_playlist" = "xyes"; then
   if test "x$have_playlist" != "xyes"; then
      AC_MSG_ERROR([Could not find Totem PL parser.])
   fi
fi

AM_CONDITIONAL(HAVE_TOTEM_PL_PARSER, test "x$have_playlist" = "xyes")

#####################################################################
# Check for tracker-extract: Guarantee existence of certain metadata
#####################################################################

# Currently guarantees (for extraction):
# - nie:title for music and video files
# - nie:contentCreated for music, video and image files

AC_ARG_ENABLE(guarantee-metadata,
              AS_HELP_STRING([--enable-guarantee-metadata],
                             [enable guaranteed existence of some metadata, e.g. nie:title from files [[default=no]]]),
              [enable_guarantee_metadata=$enableval],
              [enable_guarantee_metadata=no])

if test "x$enable_guarantee_metadata" != "xno"; then
  AC_DEFINE(GUARANTEE_METADATA, 1, [Guarantee existence of some metadata])
fi

####################################################################
# Check if we should use initctl or sysvinit
####################################################################

# Check for initcl
AC_PATH_PROG(INITCTL, initctl, [])
AC_SUBST(INITCTL)
AM_CONDITIONAL(HAVE_UPSTART, test "x$INITCTL" != "x")

####################################################################
# Check whether we should install artwork
####################################################################

AC_ARG_ENABLE(artwork,
              AS_HELP_STRING([--enable-artwork],
                             [install artwork, e.g. data/icons/* [[default=auto]]]),
              [enable_artwork=$enableval],
              [enable_artwork=auto])

if test "x$enable_artwork" != "xno"; then
   AC_DEFINE(HAVE_ARTWORK, [], [Define if we should install artwork])
   have_artwork=yes
else
   have_artwork="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_ARTWORK, test "x$have_artwork" = "xyes")


##################################################################
# Write generated files
##################################################################

AC_CONFIG_FILES([
	docs/Makefile
	docs/design/Makefile
	docs/manpages/Makefile
	docs/reference/Makefile
	docs/reference/libtracker-control/Makefile
	docs/reference/libtracker-control/version.xml
	docs/reference/libtracker-sparql/Makefile
	docs/reference/libtracker-sparql/version.xml
	docs/reference/libtracker-miner/Makefile
	docs/reference/libtracker-miner/version.xml
	docs/reference/ontology/Makefile
	docs/reference/ontology/version.xml
	docs/tools/Makefile
	Makefile
	po/Makefile.in
	src/gvdb/Makefile
	src/libtracker-common/Makefile
	src/libtracker-common/stop-words/Makefile
	src/libtracker-sparql/Makefile
	src/libtracker-sparql/tracker-sparql.pc
	src/libtracker-bus/Makefile
	src/libtracker-control/Makefile
	src/libtracker-control/tracker-control.pc
	src/libtracker-data/Makefile
	src/libtracker-direct/Makefile
	src/libtracker-sparql-backend/Makefile
	src/libtracker-fts/Makefile
	src/libtracker-extract/Makefile
	src/libtracker-miner/Makefile
	src/libtracker-miner/tracker-miner.pc
	src/Makefile
	src/miners/Makefile
	src/miners/fs/Makefile
	src/miners/apps/Makefile
	src/miners/rss/Makefile
	src/miners/user-guides/Makefile
	src/ontologies/Makefile
	src/tracker-store/Makefile
	src/tracker/Makefile
	src/tracker-extract/Makefile
	src/tracker-preferences/Makefile
	src/tracker-needle/Makefile
	src/tracker-needle/icons/Makefile
	src/tracker-needle/icons/16x16/Makefile
	src/tracker-needle/icons/22x22/Makefile
	src/tracker-needle/icons/24x24/Makefile
	src/tracker-needle/icons/32x32/Makefile
	src/tracker-needle/icons/48x48/Makefile
	src/tracker-needle/icons/scalable/Makefile
	src/tracker-writeback/Makefile
	src/plugins/Makefile
	src/plugins/evolution/Makefile
	src/plugins/thunderbird/Makefile
	src/plugins/thunderbird/install.rdf
        src/plugins/thunderbird/chrome/Makefile
        src/plugins/thunderbird/chrome/content/Makefile
        src/plugins/thunderbird/chrome/locale/Makefile
        src/plugins/thunderbird/chrome/skin/Makefile
        src/plugins/thunderbird/defaults/Makefile
        src/plugins/thunderbird/defaults/preferences/Makefile
	src/plugins/firefox/Makefile
	src/plugins/firefox/install.rdf
        src/plugins/firefox/chrome/Makefile
        src/plugins/firefox/chrome/content/Makefile
        src/plugins/firefox/chrome/locale/Makefile
        src/plugins/firefox/chrome/skin/Makefile
        src/plugins/firefox/defaults/Makefile
        src/plugins/firefox/defaults/preferences/Makefile
	src/plugins/nautilus/Makefile
	tests/common/Makefile
        tests/gvdb/Makefile
	tests/libtracker-common/Makefile
	tests/libtracker-extract/Makefile
	tests/libtracker-data/Makefile
	tests/libtracker-data/aggregates/Makefile
	tests/libtracker-data/algebra/Makefile
	tests/libtracker-data/anon/Makefile
	tests/libtracker-data/ask/Makefile
	tests/libtracker-data/basic/Makefile
	tests/libtracker-data/bind/Makefile
	tests/libtracker-data/bnode-coreference/Makefile
	tests/libtracker-data/bound/Makefile
	tests/libtracker-data/datetime/Makefile
	tests/libtracker-data/expr-ops/Makefile
	tests/libtracker-data/functions/Makefile
	tests/libtracker-data/graph/Makefile
	tests/libtracker-data/nie/Makefile
	tests/libtracker-data/nmo/Makefile
	tests/libtracker-data/mixed-cardinality/Makefile
	tests/libtracker-data/optional/Makefile
	tests/libtracker-data/regex/Makefile
	tests/libtracker-data/sort/Makefile
	tests/libtracker-data/subqueries/Makefile
	tests/libtracker-data/error/Makefile
	tests/libtracker-data/backup/Makefile
	tests/libtracker-data/turtle/Makefile
	tests/libtracker-miner/Makefile
	tests/libtracker-fts/Makefile
	tests/libtracker-fts/limits/Makefile
	tests/libtracker-fts/prefix/Makefile
	tests/libtracker-sparql/Makefile
	tests/functional-tests/Makefile
	tests/functional-tests/ipc/Makefile
	tests/functional-tests/common/Makefile
	tests/functional-tests/common/utils/configuration.py
	tests/functional-tests/common/utils/Makefile
	tests/functional-tests/unittest2/Makefile
	tests/functional-tests/test-extraction-data/Makefile
	tests/functional-tests/test-extraction-data/audio/Makefile
	tests/functional-tests/test-extraction-data/images/Makefile
	tests/functional-tests/test-extraction-data/office/Makefile
	tests/functional-tests/test-extraction-data/video/Makefile
	tests/functional-tests/test-extraction-data/playlists/Makefile
	tests/functional-tests/test-writeback-data/Makefile
	tests/functional-tests/test-ontologies/Makefile
	tests/functional-tests/test-ontologies/basic/Makefile
	tests/functional-tests/test-ontologies/property-range-int-to-string/Makefile
	tests/functional-tests/test-ontologies/property-range-string-to-date/Makefile
	tests/functional-tests/test-ontologies/cardinality/Makefile
	tests/functional-tests/test-ontologies/notify/Makefile
	tests/functional-tests/test-ontologies/indexed/Makefile
	tests/functional-tests/test-ontologies/add-class/Makefile
	tests/functional-tests/test-ontologies/add-prop/Makefile
	tests/functional-tests/test-ontologies/add-domainIndex/Makefile
	tests/functional-tests/test-ontologies/superclass-remove/Makefile
	tests/functional-tests/test-ontologies/property-promotion/Makefile
	tests/functional-tests/test-ontologies/basic-future/Makefile
	tests/functional-tests/test-apps-data/Makefile
	tests/functional-tests/ttl/Makefile
	tests/Makefile
	tests/tracker-steroids/Makefile
	tests/tracker-writeback/Makefile
	utils/Makefile
	utils/gtk-sparql/Makefile
	utils/ontology/Makefile
	utils/data-generators/Makefile
	utils/data-generators/cc/Makefile
	utils/mtp/Makefile
	utils/sandbox/Makefile
	utils/tracker-resdump/Makefile
	examples/Makefile
	examples/libtracker-miner/Makefile
	examples/libtracker-sparql/Makefile
	examples/rss-reader/Makefile
])

AC_OUTPUT

echo "
Build Configuration:

	Prefix:                                 ${prefix}
	Source code location:                   ${srcdir}
	Compiler:                               ${CC}
	Compiler Warnings:                      $enable_compile_warnings
          C    Flags:                            $warning_cflags
          VALA Flags:                            $warning_valacflags

	Win32:                                  $native_win32

	Enable gtk doc (for documentation):     $enable_gtk_doc

	Enable functional tests:                $enable_functional_tests
	Enable unit tests:                      $have_unit_tests

Feature Support:

	Support for HAL:                        $have_hal
	Support for UPower:                     $have_upower
	Support for network status detection:   $have_network_manager
	Unicode support library:                $with_unicode_support

	Build with Journal support:             $have_tracker_journal
	Build with SQLite FTS support:          $have_tracker_fts (built-in FTS: $have_builtin_fts5)
	Build with Stemming support:            $have_libstemmer

	Cache media art                         $have_libmediaart (libmediaart)
	Install artwork                         $have_artwork

	Bash completion support:                $with_bash_completion_dir ($BASH_COMPLETION_DIR)

Data Miners / Writebacks:

	FS (File System):                       $have_tracker_miner_fs (MeeGo support: $have_meegotouch)
	Applications:                           $have_tracker_miner_apps
	RSS:                                    $have_tracker_miner_rss
	User Guides:                            $have_tracker_miner_user_guides
	Email:
          Evolution:                            $have_tracker_miner_evolution ($tracker_miner_evolution_install_dir)
          Thunderbird:                          $have_tracker_miner_thunderbird ($tracker_miner_thunderbird_install_dir)
        Bookmarks:
          FireFox:                              $have_tracker_miner_firefox ($tracker_miner_firefox_install_dir)

        Extract (secondary extraction):         $have_tracker_extract
        Writeback (writing changes back):       $have_tracker_writeback"

if test "x$enable_tracker_extract" != "xno" ; then
echo "
Metadata Extractors:

	Support PNG:                            $have_libpng
	Support PDF:                            $have_poppler
	Support XPS:                            $have_libgxps
	Support GIF:                            $have_libgif (xmp: $have_exempi)
	Support JPEG:                           $have_libjpeg (xmp: $have_exempi, exif: $have_libexif, iptc: $have_libiptcdata)
	Support TIFF:                           $have_libtiff (xmp: $have_exempi, exif: yes, iptc: $have_libiptcdata)
	Support Vorbis (ogg/etc):               $have_libvorbis
	Support Flac:                           $have_libflac
	Support MS & Open Office:               $have_libgsf
	Support XML / HTML:                     $have_libxml2
	Support embedded / sidecar XMP:         $have_exempi
	Support generic media formats:          $have_generic_media_handler ($have_generic_media_handler_app) (backend: $gstreamer_backend)
	Support cue sheet parsing:              $have_libcue (libcue2.0: $have_libcue2)
	Support playlists (w/ Totem):           $have_playlist
	Support ISO image parsing:              $have_libosinfo
	Support AbiWord document parsing:       $have_abiword
	Support DVI parsing:                    $have_dvi
	Support MP3 parsing:                    $have_mp3
	Support MP3 tag charset detection:      $have_charset_detection (icu: $have_libicu_charset_detection, enca: $have_enca)
	Support PS parsing:                     $have_ps
	Support text parsing:                   $have_text
	Support icon parsing:                   $have_icon"
fi

if test "x$enable_tracker_writeback" != "xno" ; then
echo "
Writeback Formats:

	Audio files using Taglib:               $have_taglib
	XMP:                                    $have_exempi"
fi

echo "
Applications:

	Build tracker-preferences:              $have_tracker_preferences
	Build tracker-needle:                   $have_tracker_needle

Frameworks / Options:

        Support GNOME's Nautilus                $have_tracker_nautilus_extension ($tracker_nautilus_extension_install_dir)
	Support Maemo                           $enable_maemo
	Support libmeegotouch                   $have_meegotouch
	Support Guaranteed Metadata             $enable_guarantee_metadata (e.g. guess nie:title from files)

"

# Any warnings we need to issue...
if test "x$sqlite_safe" = "xno"; then
sqlite_version=$($PKG_CONFIG --modversion sqlite3)
echo "
WARNING:
        SQLite3 version $sqlite_version is not safe. For details see:
        3.7.10 - 3.7.13: https://mail.gnome.org/archives/tracker-list/2012-October/msg00028.html
        3.8.1:           https://mail.gnome.org/archives/tracker-list/2013-November/msg00021.html
        3.8.4.2:         https://mail.gnome.org/archives/tracker-list/2014-April/msg00001.html


"
fi

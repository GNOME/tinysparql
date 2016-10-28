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
m4_define([tracker_minor_version], [11])
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

AC_INIT([tracker-miner-rss],
        [tracker_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=tracker],
        [tracker-miner-rss],
        [https://wiki.gnome.org/Projects/Tracker])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/miners/rss/tracker-main.c])
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
TRACKER_REQUIRED=1.10.0
GLIB_REQUIRED=2.44.0
LIBGRSS_REQUIRED=0.7

# Needed to generate .gir files,
# see http://live.gnome.org/GnomeGoals/AddGObjectIntrospectionSupport
GOBJECT_INTROSPECTION_CHECK([0.9.5])

GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
AC_SUBST(GLIB_PREFIX)

# Check requirements for libtracker-common
LIBTRACKER_COMMON_REQUIRED="glib-2.0        >= $GLIB_REQUIRED
                            gio-unix-2.0    >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_COMMON, [$LIBTRACKER_COMMON_REQUIRED])

case $host in
  *-*-openbsd*)
    LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS -lkvm"
    ;;
esac

# Check requirements for tracker-miner-rss
TRACKER_MINER_RSS_REQUIRED="tracker-miner-1.0 >= $TRACKER_REQUIRED
                            glib-2.0 >= $GLIB_REQUIRED
                            libgrss  >= $LIBGRSS_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_RSS, [$TRACKER_MINER_RSS_REQUIRED],
                  [have_tracker_miner_rss=yes],
                  [have_tracker_miner_rss=no])

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

# Add to libtracker-common
LIBTRACKER_COMMON_CFLAGS="$LIBTRACKER_COMMON_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS $UNICODE_SUPPORT_LIBS"

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

##################################################################
# Write generated files
##################################################################

AC_CONFIG_FILES([
	Makefile
	po/Makefile.in
	src/Makefile
        src/libtracker-common/Makefile
	src/miners/Makefile
	src/miners/rss/Makefile
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

	Enable functional tests:                $enable_functional_tests
	Enable unit tests:                      $have_unit_tests"

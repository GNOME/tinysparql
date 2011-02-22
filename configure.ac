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
m4_define([tracker_major_version], [0])
m4_define([tracker_minor_version], [10])
m4_define([tracker_micro_version], [0])
m4_define([tracker_interface_age], [0])
m4_define([tracker_binary_age],
          [m4_eval(100 * tracker_minor_version + tracker_micro_version)])
m4_define([tracker_version],
          [tracker_major_version.tracker_minor_version.tracker_micro_version])
# This is the X.Y used in -llibtracker-FOO-X.Y
m4_define([tracker_api_version], [0.10])

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
m4_define([tracker_binary_version], [0.10.0])

AC_INIT([tracker],
        [tracker_version],
        [http://bugzilla.gnome.org/],
        [tracker],
        [http://www.tracker-project.org])

AC_CONFIG_SRCDIR([src/tracker-store/tracker-main.vala])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11 no-define tar-ustar -Wno-portability])

AM_SILENT_RULES([yes])

IDT_COMPILE_WARNINGS

TRACKER_MAJOR_VERSION=tracker_major_version
TRACKER_MINOR_VERSION=tracker_minor_version
TRACKER_MICRO_VERSION=tracker_micro_version
TRACKER_INTERFACE_AGE=tracker_interface_age
TRACKER_BINARY_AGE=tracker_binary_age
TRACKER_VERSION=tracker_version
TRACKER_API_VERSION=tracker_api_version
TRACKER_BINARY_VERSION=tracker_binary_version
AC_SUBST(TRACKER_MAJOR_VERSION)
AC_SUBST(TRACKER_MINOR_VERSION)
AC_SUBST(TRACKER_MICRO_VERSION)
AC_SUBST(TRACKER_INTERFACE_AGE)
AC_SUBST(TRACKER_BINARY_AGE)
AC_SUBST(TRACKER_API_VERSION)
AC_SUBST(TRACKER_VERSION)
AC_SUBST(TRACKER_BINARY_VERSION)


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
AC_PROG_CC
AM_PROG_CC_C_O()
AC_PROG_CXX
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h linux/unistd.h sys/statvfs.h])

AC_CHECK_HEADER([zlib.h],
                [],
                [AC_MSG_ERROR([You must have zlib.h and zlib installed])])

# Can posix_fadvise be used
AC_CHECK_DECLS(posix_fadvise, [], [], [
#define _XOPEN_SOURCE 600
#include <fcntl.h>])

# Checks for functions
AC_CHECK_FUNCS([posix_fadvise])
AC_CHECK_FUNCS([getline])

CFLAGS="$CFLAGS"

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

# Check ioprio support
AC_CACHE_CHECK([if we have ioprio],[tracker_cv_have_ioprio],
   [AC_RUN_IFELSE(
      [AC_LANG_PROGRAM([[
         #include <stdlib.h>
         #include <errno.h>
         #include <sys/syscall.h>
         #include <unistd.h>
         inline int ioprio_get (int which, int who)
         {
             return syscall (__NR_ioprio_get, which, who);
         }
         ]],
         [[
             return !(ioprio_get (1, 0) != -1);
         ]])],
   [tracker_cv_have_ioprio=yes],
   [tracker_cv_have_ioprio=no],
   [AC_MSG_ERROR([cross-compiling: please set 'tracker_cv_have_ioprio'])])
])

if test "x$tracker_cv_have_ioprio" = "xyes" ; then
   AC_DEFINE(HAVE_IOPRIO, 1, [Define if we have ioprio])
fi

# Binary required versions
PYTHON_REQUIRED=2.6

# Library required versions
DBUS_REQUIRED=1.3.1
DBUS_GLIB_REQUIRED=0.82
GLIB_REQUIRED=2.26.0
PANGO_REQUIRED=1.0.0
GTK_REQUIRED=2.18.0
LIBXML2_REQUIRED=2.6
LIBNOTIFY_REQUIRED=0.4.3
HAL_REQUIRED=0.5
UPOWER_REQUIRED=0.9.0
GDKPIXBUF_REQUIRED=2.12.0
QT_REQUIRED=4.7.1
MEEGOTOUCH_REQUIRED=0.20
POPPLER_REQUIRED=0.12.2
CAIRO_REQUIRED=1.0
GDK_REQUIRED=1.0
LIBVORBIS_REQUIRED=0.22
LIBFLAC_REQUIRED=1.2.1
LIBEXIF_REQUIRED=0.6
LIBGSF_REQUIRED=1.13
EXEMPI_REQUIRED=2.1.0
EVO_REQUIRED=2.91.90
EVO_SHELL_REQUIRED=2.91.90
EDS_REQUIRED=2.91.90
CAMEL_REQUIRED=2.91.90
# Unlikely version for now, Nepomuk integration isn't finished in streamanalyzer atm
LIBSTREAMANALYZER_REQUIRED=0.7.0
GEE_REQUIRED=0.3
TAGLIB_REQUIRED=1.6
GNOME_KEYRING_REQUIRED=2.26
LIBGRSS_REQUIRED=0.3
REST_REQUIRED=0.6
NETWORK_MANAGER_REQUIRED=0.8

# 3.6.11 for sqlite_backup API
# 3.6.16 to fix test failures
# 3.6.17 for shared cache mode with virtual tables
# 3.7.0 for WAL
SQLITE_REQUIRED=3.7.0

# NOTE: dbus and dbus-glib-1 are only used now for libtracker-client
# which is deprecated and tracker-writeback which will be refactored
# shortly to use gdbus. The other place which requires dbus-glib-1 is
# for HAL used in tracker-miner-fs if the alternative libraries are
# not available.

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

# Check requirements for libtracker-client
LIBTRACKER_CLIENT_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gobject-2.0  >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED
                            dbus-1       >= $DBUS_REQUIRED
                            dbus-glib-1  >= $DBUS_GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_CLIENT, [$LIBTRACKER_CLIENT_REQUIRED])

LIBTRACKER_CLIENT_CFLAGS="$LIBTRACKER_CLIENT_CFLAGS -Wno-deprecated-declarations"

# Check requirements for libtracker-common
LIBTRACKER_COMMON_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_COMMON, [$LIBTRACKER_COMMON_REQUIRED])

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
                             gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_EXTRACT, [$LIBTRACKER_EXTRACT_REQUIRED])

# Check requirements for libtracker-fts
LIBTRACKER_FTS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                         gio-unix-2.0 >= $GLIB_REQUIRED
                         gthread-2.0  >= $GLIB_REQUIRED
                         sqlite3      >= $SQLITE_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_FTS, [$LIBTRACKER_FTS_REQUIRED])

# Check requirements for libtracker-miner
LIBTRACKER_MINER_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                           gio-unix-2.0 >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(LIBTRACKER_MINER, [$LIBTRACKER_MINER_REQUIRED])

# Check requirements for libtracker-sparql
LIBTRACKER_SPARQL_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED
                            uuid"

PKG_CHECK_MODULES(LIBTRACKER_SPARQL, [$LIBTRACKER_SPARQL_REQUIRED])

# Check requirements for tracker-control
TRACKER_CONTROL_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                          gio-unix-2.0 >= $GLIB_REQUIRED
                          gthread-2.0  >= $GLIB_REQUIRED
                          uuid"

PKG_CHECK_MODULES(TRACKER_CONTROL, [$TRACKER_CONTROL_REQUIRED])

# Check requirements for tracker-explorer
TRACKER_EXPLORER_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                           gio-unix-2.0 >= $GLIB_REQUIRED
                           gthread-2.0  >= $GLIB_REQUIRED
                           gmodule-2.0  >= $GLIB_REQUIRED
                           gtk+-2.0     >= $GTK_REQUIRED
			   gee-1.0      >= $GEE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_EXPLORER, [$TRACKER_EXPLORER_REQUIRED],
                  [have_tracker_explorer=yes],
                  [have_tracker_explorer=no])

TRACKER_RESDUMP_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                          gee-1.0      >= $GEE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_RESDUMP, [$TRACKER_RESDUMP_REQUIRED],
                  [have_tracker_resdump=yes],
                  [have_tracker_resdump=no])

# Check requirements for tracker-store
TRACKER_STORE_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                        gio-unix-2.0 >= $GLIB_REQUIRED
                        gthread-2.0  >= $GLIB_REQUIRED
                        gmodule-2.0  >= $GLIB_REQUIRED
                        sqlite3      >= $SQLITE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_STORE, [$TRACKER_STORE_REQUIRED])

TRACKER_STORE_LIBS="$TRACKER_STORE_LIBS -lz -lm"

# Check requirements for tracker-extract
TRACKER_EXTRACT_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                          gio-unix-2.0 >= $GLIB_REQUIRED
                          gthread-2.0  >= $GLIB_REQUIRED
                          gmodule-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_EXTRACT, [$TRACKER_EXTRACT_REQUIRED])

# Check requirements for tracker-extract modules
TRACKER_EXTRACT_MODULES_REQUIRED="glib-2.0     >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_EXTRACT_MODULES, [$TRACKER_EXTRACT_MODULES_REQUIRED])

# Check requirements for tracker-writeback
TRACKER_WRITEBACK_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            gio-unix-2.0 >= $GLIB_REQUIRED
                            gthread-2.0  >= $GLIB_REQUIRED
                            gmodule-2.0  >= $GLIB_REQUIRED
                            dbus-1       >= $DBUS_REQUIRED
                            dbus-glib-1  >= $DBUS_GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_WRITEBACK, [$TRACKER_WRITEBACK_REQUIRED])

# Check requirements for tracker-miner-fs
TRACKER_MINER_FS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                           gio-unix-2.0 >= $GLIB_REQUIRED
                           gthread-2.0  >= $GLIB_REQUIRED
                           gmodule-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_FS, [$TRACKER_MINER_FS_REQUIRED])

TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS -lz -lm"

# Check requirements for tracker-miner-evolution
evolution_plugin_name="evolution-plugin"

TRACKER_MINER_EVOLUTION_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                  evolution-shell           >= $EVO_SHELL_REQUIRED
                                  evolution-plugin          >= $EVO_REQUIRED
                                  evolution-data-server-1.2 >= $EDS_REQUIRED
                                  camel-1.2                 >= $CAMEL_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_REQUIRED],
                  [have_tracker_miner_evolution=yes],
                  [have_tracker_miner_evolution=no])

if test "x$have_tracker_miner_evolution" != "xyes"; then
   evolution_plugin_name="evolution-plugin-3.0"

   TRACKER_MINER_EVOLUTION_REQUIRED="glib-2.0                  >= $GLIB_REQUIRED
                                     evolution-plugin-3.0      >= $EVO_REQUIRED
                                     evolution-data-server-1.2 >= $EDS_REQUIRED"

   PKG_CHECK_MODULES(TRACKER_MINER_EVOLUTION, [$TRACKER_MINER_EVOLUTION_REQUIRED],
                     [have_tracker_miner_evolution=yes],
                     [have_tracker_miner_evolution=no])
fi

# Check requirements for tracker-miner-rss
TRACKER_MINER_RSS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                            libgrss-0    >= $LIBGRSS_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_RSS, [$TRACKER_MINER_RSS_REQUIRED],
                  [have_tracker_miner_rss=yes],
                  [have_tracker_miner_rss=no])

# Check requirements for tracker-miner-flickr
TRACKER_MINER_FLICKR_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                               rest-0.6     >= $REST_REQUIRED"

PKG_CHECK_MODULES(TRACKER_MINER_FLICKR, [$TRACKER_MINER_FLICKR_REQUIRED],
                  [have_tracker_miner_flickr=yes],
                  [have_tracker_miner_flickr=no])

TRACKER_MINER_FLICKR_LIBS="$TRACKER_MINER_FLICKR_LIBS -lz -lm"

# Check requirements for tracker-utils
TRACKER_UTILS_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                        gio-unix-2.0 >= $GLIB_REQUIRED
                        gthread-2.0  >= $GLIB_REQUIRED"

PKG_CHECK_MODULES(TRACKER_UTILS, [$TRACKER_UTILS_REQUIRED])

# Check requirements for tracker-needle
TRACKER_NEEDLE_REQUIRED="glib-2.0     >= $GLIB_REQUIRED
                         gio-unix-2.0 >= $GLIB_REQUIRED
                         gthread-2.0  >= $GLIB_REQUIRED
                         gmodule-2.0  >= $GLIB_REQUIRED
                         gtk+-2.0     >= $GTK_REQUIRED"

PKG_CHECK_MODULES(TRACKER_NEEDLE, [$TRACKER_NEEDLE_REQUIRED],
                  [have_tracker_needle=yes],
                  [have_tracker_needle=no])

# Check requirements for tracker-preferences
TRACKER_PREFERENCES_REQUIRED="glib-2.0       >= $GLIB_REQUIRED
                              gio-unix-2.0   >= $GLIB_REQUIRED
                              gthread-2.0    >= $GLIB_REQUIRED
                              gmodule-2.0    >= $GLIB_REQUIRED
                              gtk+-2.0       >= $GTK_REQUIRED"

PKG_CHECK_MODULES(TRACKER_PREFERENCES, [$TRACKER_PREFERENCES_REQUIRED],
                  [have_tracker_preferences=yes],
                  [have_tracker_preferences=no])

# Check requirements for tracker-search-bar
TRACKER_SEARCH_BAR_REQUIRED="glib-2.0       >= $GLIB_REQUIRED
                             gio-unix-2.0   >= $GLIB_REQUIRED
                             gthread-2.0    >= $GLIB_REQUIRED
                             gmodule-2.0    >= $GLIB_REQUIRED
                             gtk+-3.0       >= $GTK_REQUIRED
                             gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED
                             pango          >= $PANGO_REQUIRED
			     gee-1.0        >= $GEE_REQUIRED
                             libpanelapplet-4.0"

PKG_CHECK_MODULES(TRACKER_SEARCH_BAR, [$TRACKER_SEARCH_BAR_REQUIRED],
                  [have_tracker_search_bar=yes],
                  [have_tracker_search_bar=no])

# Check requirements for Nautilus extension
TRACKER_NAUTILUS_EXTENSION_REQUIRED="glib-2.0       >= $GLIB_REQUIRED
                                     gio-unix-2.0   >= $GLIB_REQUIRED
                                     gthread-2.0    >= $GLIB_REQUIRED
                                     gtk+-2.0       >= $GTK_REQUIRED
                                     libnautilus-extension"

PKG_CHECK_MODULES(TRACKER_NAUTILUS_EXTENSION, [$TRACKER_NAUTILUS_EXTENSION_REQUIRED],
                  [have_tracker_nautilus_extension=yes],
                  [have_tracker_nautilus_extension=no])

# General VALAFLAGS/CFLAGS/LIBS
BUILD_VALAFLAGS="-g --target-glib=2.26"
BUILD_CFLAGS="${WARN_CFLAGS} ${GCOV_CFLAGS} -DG_LOG_DOMAIN=\\\"Tracker\\\" -DTRACKER_COMPILATION"
BUILD_LIBS="${GCOV_LIBS}"

AC_SUBST(BUILD_VALAFLAGS)
AC_SUBST(BUILD_CFLAGS)
AC_SUBST(BUILD_LIBS)

# Check we have Vala valac command we need
AC_PATH_PROG(VALAC, valac, valac)
if test -z $VALAC; then
   AC_MSG_ERROR([Could not find 'valac'])
fi

AC_SUBST(VALAC)

####################################################################
# Check gettext/intltool support
####################################################################
IT_PROG_INTLTOOL([0.40.0])

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The gettext translation domain])

####################################################################
# Check if we should enable GCOV coverage reporting support
####################################################################

AC_ARG_ENABLE(gcov,
              AS_HELP_STRING([--enable-gcov],
                             [enable coverage reporting support [[default=no]]]),,
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
# Check for functional-test include/exclude
####################################################################
AC_ARG_ENABLE([functional-tests],
              AS_HELP_STRING([--enable-functional-tests],
                             [enable functional tests [[default=no]]]),,
              [enable_functional_tests=no])

if test x$enable_functional_tests != "xno"; then
   # Python check, require >= 2.6
   AC_PATH_PROG([PYTHON],[python],[:])
   AS_IF([test "$PYTHON" != ":"],
         [AM_PYTHON_CHECK_VERSION([$PYTHON],[$PYTHON_REQUIRED],[:],[PYTHON=":"])])

   if test "$PYTHON" = ":"; then
      AC_MSG_ERROR([Couldn't find Python >= $PYTHON_REQUIRED (for functional-tests, try --disable-functional-tests).])
   fi

   AC_DEFINE(DIST_FUNCTIONAL_TESTS, 1, [Include functional tests in the installation])
fi

AM_CONDITIONAL(DIST_FUNCTIONAL_TESTS, test "x$enable_functional_tests" != "xno")

####################################################################
# Check for gtk-doc and docbook-tools
####################################################################

GTK_DOC_CHECK([1.8])

if test "x$enable_gtk_doc" != "xno"; then
   AC_DEFINE(HAVE_GTK_DOC, 1, [Define if we have gtk-doc (with gtk-doc)])

   # Check for dia if we are building gtk_doc
   AC_PATH_PROG(DIA, dia)
   AC_SUBST(DIA)

   if test -z "$DIA"; then
      AC_MSG_ERROR([Could not find Dia tool required to build documentation.])
   fi

   # Check for graphviz if we are building gtk_doc
   AC_PATH_PROG(GRAPHVIZ_FDP, fdp)
   AC_SUBST(GRAPHVIZ_FDP)

   if test -z "$GRAPHVIZ_FDP"; then
      AC_MSG_ERROR([Couldn't find Graphviz's fdp tool (graphviz).])
   fi
fi

AM_CONDITIONAL(HAVE_GTK_DOC, test "$enable_gtk_doc" = "yes")
AM_CONDITIONAL(HAVE_GRAPHVIZ_FDP, test -n "$GRAPHVIZ_FDP")

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
             [enable_maemo="$enableval"],
             [enable_maemo=no])

AM_CONDITIONAL(HAVE_MAEMO, test "x$enable_maemo" = "xyes")

if test "x$enable_maemo" = "xyes" ; then

   AC_DEFINE(HAVE_MAEMO, 1, [Define if we enable Maemo specific features])

   PKG_CHECK_MODULES(GCONF,
                     [gconf-2.0],
                     [have_gconf=yes],
                     [have_gconf=no])

   LIBTRACKER_COMMON_CFLAGS="$LIBTRACKER_COMMON_CFLAGS $GCONF_CFLAGS"
   LIBTRACKER_COMMON_LIBS="$LIBTRACKER_COMMON_LIBS $GCONF_LIBS"

   if test "x$have_gconf" != "xyes"; then
      AC_MSG_ERROR([Couldn't find gconf, needed when enabling maemo support])
   fi
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

##################################################################
# Check for tracker-fts, allow disabling FTS support
##################################################################

AC_ARG_ENABLE([tracker-fts],
              AS_HELP_STRING([--enable-tracker-fts],
                             [enable the tracker FTS [[default=auto]]]),,
              [enable_tracker_fts=auto])

if test "x$enable_tracker_fts" != "xno" ; then
   have_tracker_fts="yes"
else
   have_tracker_fts="no  (disabled)"
fi

if test "x$have_tracker_fts" = "xyes"; then
   AC_DEFINE(HAVE_TRACKER_FTS, [1], [Define to 1 if tracker FTS is compiled])
else
   AC_DEFINE(HAVE_TRACKER_FTS, [0], [Define to 0 if tracker FTS is not compiled])
fi

AM_CONDITIONAL(HAVE_TRACKER_FTS, test "$have_tracker_fts" = "yes")

####################################################################
# Check for D-Bus requirements
####################################################################

# Check we are not using a different D-Bus services dir
AC_ARG_WITH([session_bus_services_dir],
            AS_HELP_STRING([--with-session-bus-services-dir],
                           [path to DBus services directory]))

if test "x$with_session_bus_services_dir" = "x" ; then
   services_dir="$datadir/dbus-1/services"
else
   services_dir="$with_session_bus_services_dir"
fi

DBUS_SERVICES_DIR="$services_dir"
AC_SUBST(DBUS_SERVICES_DIR)

# Check we have the D-Bus binding tool we need
AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
if test -z $DBUSBINDINGTOOL; then
   AC_MSG_ERROR([Could not find 'dbus-binding-tool'])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

####################################################################
# Should we build GLib based unit tests
####################################################################

AC_ARG_ENABLE(unit_tests,
              AS_HELP_STRING([--enable-unit-tests],
                             [enable unit tests [[default=yes]]]), ,
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
                             [disable UPower support for AC power detection [[default=auto]]]),,
              [enable_upower=auto])

if test "x$enable_upower" != "xno"; then
   PKG_CHECK_MODULES(UPOWER,
                     [upower-glib >= $UPOWER_REQUIRED],
                     [have_upower=yes],
                     [have_upower=no])

   TRACKER_MINER_FS_CFLAGS="$TRACKER_MINER_FS_CFLAGS $UPOWER_CFLAGS"
   TRACKER_MINER_FS_LIBS="$TRACKER_MINER_FS_LIBS $UPOWER_LIBS"

   if test "x$have_upower" = "xyes"; then
      AC_DEFINE(HAVE_UPOWER, [], [Define if we have UPOWER])
   fi
else
   have_upower="no  (disabled)"
fi

if test "x$enable_upower" = "xyes"; then
   if test "x$have_upower" != "xyes"; then
      AC_MSG_ERROR([Couldn't find upower >= $UPOWER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_UPOWER, test "x$have_upower" = "xyes")

##################################################################
# Check for libtracker-common: hal for battery/power support (old)
##################################################################

AC_ARG_ENABLE(hal,
              AS_HELP_STRING([--disable-hal],
                             [disable HAL support for AC power detection [[default=auto]]]),,
              [enable_hal=auto])

if test "x$have_upower" != "xyes" && test "x$enable_hal" != "xno"; then
   PKG_CHECK_MODULES(HAL,
                     [dbus-1      >= $DBUS_REQUIRED
                      dbus-glib-1 >= $DBUS_GLIB_REQUIRED
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
      AC_MSG_ERROR([Couldn't find hal >= $HAL_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_HAL, test "x$have_hal" = "xyes")

##################################################################
# Check for libtracker-data and libtracker-fts: Unicode support
#
# By default, AUTO with this order of preference:
#  1)  libunistring
#  2)  libicu
#
# Note that glib can only be used if explicitly requested for it.
##################################################################

# Check for libunistring...
AC_CHECK_HEADER(uniwbrk.h,
                AC_CHECK_LIB([unistring],
                             [u8_wordbreaks],
                             [have_libunistring=yes],
                             [have_libunistring=no]))
LIBUNISTRING_CFLAGS=""
LIBUNISTRING_LIBS="-lunistring"

# Check for libicu... Note that AC_CHECK_LIB cannot be used as
#  symbol name includes libicu library version... don't want to
#  look for ubrk_next_4_2
AC_CHECK_HEADER(unicode/ubrk.h,
                [have_libicu=yes],
                [have_libicu=no])
LIBICU_CFLAGS=""
LIBICU_LIBS="-licuuc -licui18n"

# Check for Pango...
PKG_CHECK_MODULES(PANGO,
                  [pango >= $PANGO_REQUIRED],
                  [have_pango=yes],
                  [have_pango=no])

# Configure option
AC_ARG_WITH([unicode-support],
            AS_HELP_STRING([--with-unicode-support],
                           [Unicode support library? (libunistring|libicu|glib) [[default=auto (!glib)]]]),,
            [with_unicode_support=auto])

# If auto, decide ourselves
if test "x$with_unicode_support" = "xauto"; then
   if test "x$have_libunistring" = "xyes"; then
      with_unicode_support=libunistring
   else
      if test "x$have_libicu" = "xyes"; then
         with_unicode_support=libicu
      else
         AC_MSG_ERROR([Couldn't find neither libunistring nor libicu. You can still compile tracker
                       without any of these two, using glib instead, but be warned that the FTS parsing
                       performance will be much worse. If you still want to use glib as Unicode support
                       library, enable it with an explicit --with-unicode-support=glib])
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
           AC_MSG_ERROR([Couldn't find libunistring])
        fi
     ;;
     # Use libicu
     "xlibicu")
        UNICODE_SUPPORT_CFLAGS=$LIBICU_CFLAGS
        UNICODE_SUPPORT_LIBS=$LIBICU_LIBS
        if test "x$have_libicu" = "xyes"; then
          AC_DEFINE(HAVE_LIBICU, [1], [libicu Unicode support library])
        else
          AC_MSG_ERROR([Couldn't find libicu])
        fi
     ;;
     # Use glib/pango
     "xglib")
        UNICODE_SUPPORT_CFLAGS=$PANGO_CFLAGS
        UNICODE_SUPPORT_LIBS=$PANGO_LIBS
        if test "x$have_pango" != "xyes"; then
          AC_MSG_ERROR([Couldn't find pango])
        fi
     ;;
     # Invalid option value
     *) AC_MSG_ERROR([Wrong value for --with-unicode-support: $with_unicode_support]) ;;
esac

# Add to libtracker-fts
LIBTRACKER_FTS_CFLAGS="$LIBTRACKER_FTS_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_FTS_LIBS="$LIBTRACKER_FTS_LIBS $UNICODE_SUPPORT_LIBS"

# Add to libtracker-data
LIBTRACKER_DATA_CFLAGS="$LIBTRACKER_DATA_CFLAGS $UNICODE_SUPPORT_CFLAGS"
LIBTRACKER_DATA_LIBS="$LIBTRACKER_DATA_LIBS $UNICODE_SUPPORT_LIBS"

AM_CONDITIONAL(BUILD_LIBUNISTRING_PARSER, test "x$with_unicode_support" = "xlibunistring")
AM_CONDITIONAL(BUILD_LIBICU_PARSER, test "x$with_unicode_support" = "xlibicu")

##################################################################
# Check for libtracker-miner: gnome-keyring for web miner details
##################################################################

AC_ARG_ENABLE(gnome_keyring,
              AS_HELP_STRING([--enable-gnome-keyring],
                             [enable Gnome Keyring support to store credentials [[default=auto]]]),,
              [enable_gnome_keyring=auto])

if test "x$enable_gnome_keyring" != "xno"; then
   PKG_CHECK_MODULES(GNOME_KEYRING,
                     [gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED],
                     [have_gnome_keyring=yes],
                     [have_gnome_keyring=no])

   LIBTRACKER_MINER_CFLAGS="$LIBTRACKER_MINER_CFLAGS $GNOME_KEYRING_CFLAGS"
   LIBTRACKER_MINER_LIBS="$LIBTRACKER_MINER_LIBS $GNOME_KEYRING_LIBS"

   if test "x$have_gnome_keyring" = "xyes"; then
      AC_DEFINE(HAVE_GNOME_KEYRING, [], [Define if we have Gnome Keyring for password storage])
   fi
fi

if test "x$enable_gnome_keyring" = "xyes"; then
   if test "x$have_gnome_keyring" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Gnome Keyring >= $GNOME_KEYRING_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_GNOME_KEYRING, test "x$have_gnome_keyring" = "xyes")

##################################################################
# Check for libtracker-miner: network-manager for web miners
##################################################################

AC_ARG_ENABLE(network_manager,
              AS_HELP_STRING([--enable-network-manager],
                             [enable NetworkManager support to detect network status [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libnm-glib >= $NETWORK_MANAGER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_NETWORK_MANAGER, test "x$have_network_manager" = "xyes")

##################################################################
# Check for libtracker-extract: libexif
##################################################################

AC_ARG_ENABLE(libexif,
              AS_HELP_STRING([--enable-libexif],
                             [enable extractor for exif data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libexif >= $LIBEXIF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_EXIF, test "x$have_libexif" = "xyes")

##################################################################
# Check for libtracker-extract: libiptcdata
##################################################################

AC_ARG_ENABLE(libiptcdata,
              AS_HELP_STRING([--enable-libiptcdata],
                             [enable extractor for iptc data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libiptcdata.])
   fi
fi

AM_CONDITIONAL(HAVE_IPTC, test "x$have_libiptcdata" = "xyes")

##################################################################
# Check for libtracker-extract, tracker-writeback: exempi
##################################################################

AC_ARG_ENABLE(exempi,
              AS_HELP_STRING([--enable-exempi],
                             [enable extractor for XMP data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find exempi >= $EXEMPI_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_EXEMPI, test "x$have_exempi" = "xyes")

####################################################################
# Check for tracker-miner-fs: meegotouch, for MeeGo desktop files
####################################################################

if test "x$enable_maemo" != "xno"; then
   PKG_CHECK_MODULES(MEEGOTOUCH,
                     [meegotouch >= $MEEGOTOUCH_REQUIRED],
                     [have_meegotouch=yes],
                     [have_meegotouch=no])

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
# Check for tracker-miner-flickr
##################################################################

AC_ARG_ENABLE(miner_flickr,
              AS_HELP_STRING([--enable-miner-flickr],
                             [enable Flickr miner [[default=auto]]]),,
                             [enable_tracker_miner_flickr=auto])

enable_tracker_miner_flickr=$enable_miner_flickr

if test "x$enable_tracker_miner_flickr" = "xyes"; then
   if test "x$have_tracker_miner_flickr" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-miner-flickr dependencies ($TRACKER_MINER_FLICKR_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_flickr" = "xno"; then
      have_tracker_miner_flickr="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_MINER_FLICKR, test "x$have_tracker_miner_flickr" = "xyes")

##################################################################
# Check for tracker-miner-rss
##################################################################

AC_ARG_ENABLE([miner_rss],
              AS_HELP_STRING([--enable-miner-rss],
                             [enable RSS data miner [[default=auto]]]),,
              [enable_tracker_miner_rss=auto])

enable_tracker_miner_rss=$enable_miner_rss

if test "x$enable_tracker_miner_rss" = "xyes" ; then
   if test "x$have_tracker_miner_rss" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-miner-rss dependencies ($TRACKER_MINER_RSS_REQUIRED).])
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
                             [enable Evolution email data miner [[default=auto]]]),,
              [enable_miner_evolution=auto])

enable_tracker_miner_evolution=$enable_miner_evolution

if test "x$enable_tracker_miner_evolution" = "xyes"; then
   if test "x$have_tracker_miner_evolution" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Tracker Evolution plugin ($TRACKER_MINER_EVOLUTION_REQUIRED).])
   fi
else
   if test "x$enable_tracker_miner_evolution" = "xno"; then
      have_tracker_miner_evolution="no  (disabled)"
   fi
fi

if test "x$have_tracker_miner_evolution" = "xyes"; then
   AC_ARG_WITH([evolution_plugin_dir],
               AS_HELP_STRING([--with-evolution-plugin-dir],
                              [path to Evolution plugin directory]))
   if test "x$with_evolution_plugin_dir" = "x" ; then
      tracker_miner_evolution_install_dir=`$PKG_CONFIG $evolution_plugin_name --variable=plugindir`
   else
      tracker_miner_evolution_install_dir="$with_tracker_miner_evolution_install_dir"
   fi

   if test -z $tracker_miner_evolution_install_dir; then
      AC_MSG_ERROR([Couldn't find evolution plugin dir, consider using --with-evolution-plugin-dir.])
   fi
fi

TRACKER_MINER_EVOLUTION_INSTALL_DIR=$tracker_miner_evolution_install_dir
AC_SUBST(TRACKER_MINER_EVOLUTION_INSTALL_DIR)

AM_CONDITIONAL(HAVE_TRACKER_MINER_EVOLUTION, test "x$have_tracker_miner_evolution" = "xyes")

####################################################################
# Check for nautilus extension
####################################################################

AC_ARG_ENABLE([nautilus-extension],
              AS_HELP_STRING([--enable-nautilus-extension],
                             [enable the nautilus extension [[default=auto]]]),,
              [enable_tracker_nautilus=auto])

enable_tracker_nautilus_extension=$enable_nautilus_extension

if test "x$enable_tracker_nautilus_extension" = "xyes"; then
   if test "x$have_tracker_nautilus_extension" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Tracker Nautilus extension dependencies ($TRACKER_NAUTILUS_EXTENSION_REQUIRED).])
   fi
else
   if test "x$enable_tracker_nautilus_extension" = "xno"; then
     have_tracker_nautilus_extension="no  (disabled)"
   fi
fi

if test "x$have_tracker_nautilus_extension" = "xyes"; then
   AC_ARG_WITH([nautilus-extensions-dir],
                  AS_HELP_STRING([--with-nautilus-extensions-dir],
                                 [path to Nautilus extensions directory]))

   if test "x$with_nautilus_extensions_dir" = "x"; then
      tracker_nautilus_extension_install_dir=`$PKG_CONFIG --variable=extensiondir libnautilus-extension`
   else
      tracker_nautilus_extension_install_dir="$with_nautilus_extension_install_dir"
   fi

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
                             [enable writeback for audio files [[default=auto]]]),,
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

#################################################################
# Check for tracker-search-bar
##################################################################

AC_ARG_ENABLE([tracker-search-bar],
              AS_HELP_STRING([--enable-tracker-search-bar],
                             [enable tracker-search-bar[[default=auto]]]),,
              [enable_tracker_search_bar=auto])

if test "x$enable_tracker_search_bar" = "xyes" ; then
   if test "x$have_tracker_search_bar" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-search-bar dependencies ($TRACKER_SEARCH_BAR_REQUIRED).])
   fi
else
   if test "x$enable_tracker_search_bar" = "xno"; then
      have_tracker_search_bar="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_SEARCH_BAR, test "$have_tracker_search_bar" = "yes")

##################################################################
# Check for tracker-needle
##################################################################

AC_ARG_ENABLE([tracker-needle],
              AS_HELP_STRING([--enable-tracker-needle],
                             [enable tracker-needle [[default=auto]]]),,
              [enable_tracker_needle=auto])

if test "x$enable_tracker_needle" = "xyes" ; then
   if test "x$have_tracker_needle" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-needle dependencies ($TRACKER_NEEDLE_REQUIRED).])
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
                             [enable the tracker preferences dialog [[default=auto]]]),,
              [enable_tracker_preferences=auto])

if test "x$enable_tracker_preferences" = "xyes" ; then
   if test "x$have_tracker_preferences" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-preferences dependencies ($TRACKER_PREFERENCES_REQUIRED).])
   fi
else
   if test "x$enable_tracker_preferences" = "xno"; then
      have_tracker_preferences="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_PREFERENCES, test "$have_tracker_preferences" = "yes")

##################################################################
# Check for tracker-explorer
##################################################################

AC_ARG_ENABLE([tracker-explorer],
              AS_HELP_STRING([--enable-tracker-explorer],
                             [enable tracker-explorer[[default=auto]]]),,
              [enable_tracker_explorer=auto])

if test "x$enable_tracker_explorer" = "xyes" ; then
   if test "x$have_tracker_explorer" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-explorer dependencies ($TRACKER_EXPLORER_REQUIRED).])
   fi
else
   if test "x$enable_tracker_explorer" = "xno"; then
      have_tracker_explorer="no  (disabled)"
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_EXPLORER, test "$have_tracker_explorer" = "yes")

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

AC_ARG_WITH(enca,
            AS_HELP_STRING([--with-enca],
                           [enable libenca for Cyrillic language detection in MP3s [[default=auto]]]),,
            [enable_enca=auto])

if test "x$enable_enca" != "xno" ; then
   PKG_CHECK_MODULES(ENCA, [enca >= 1.9], have_enca=yes, have_enca=no)

   AC_SUBST(ENCA_CFLAGS)
   AC_SUBST(ENCA_LIBS)

   if test "x$have_enca" = "xyes"; then
     AC_DEFINE(HAVE_ENCA, [], [Enca language detection aid])
   fi
else
   have_enca="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_ENCA, test "$have_enca" = "yes")

####################################################################
# Check for tracker-extract: libstreamanalyzer
####################################################################

AC_ARG_ENABLE(libstreamanalyzer,
              AS_HELP_STRING([--enable-libstreamanalyzer],
                             [enable libstreamananalyzer [[default=no]]]),,
              [enable_libstreamanalyzer=no])

if test "x$enable_libstreamanalyzer" != "xno"; then
   PKG_CHECK_MODULES(LIBSTREAMANALYZER,
                     [libstreamanalyzer >= $LIBSTREAMANALYZER_REQUIRED],
                     [have_libstreamanalyzer=yes],
                     [have_libstreamanalyzer=no])

   TRACKER_EXTRACT_CFLAGS="$TRACKER_EXTRACT_CFLAGS $LIBSTREAMANALYZER_CFLAGS"
   TRACKER_EXTRACT_LIBS="$TRACKER_EXTRACT_LIBS $LIBSTREAMANALYZER_LIBS"

   if test "x$have_libstreamanalyzer" = "xyes"; then
      AC_DEFINE(HAVE_LIBSTREAMANALYZER, [], [Define if we have libstreamanalyzer])
   fi
else
   have_libstreamanalyzer="no  (disabled)"
fi

if test "x$enable_libstreamanalyzer" = "xyes"; then
   if test "x$have_libstreamanalyzer" != "xyes"; then
      AC_MSG_ERROR([Couldn't find libstreamanalyzer >= $LIBSTREAMANALYZER_REQUIRED and libstreamanalyzer.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBSTREAMANALYZER, test "$have_libstreamanalyzer" = "yes")

####################################################################
# Check for tracker-extract: tagreadbin for gstreamer extraction
####################################################################

AC_ARG_ENABLE(gstreamer-tagreadbin,
             AS_HELP_STRING([--enable-gstreamer-tagreadbin],
                            [enable tagreadbin in gstreamer [[default=no]]]),
                            [enable_tagreadbin=yes],
                            [enable_tagreadbin=no])

if test "x$enable_tagreadbin" != "xno"; then
  AC_DEFINE(TRACKER_EXTRACT_GSTREAMER_USE_TAGREADBIN, 1, [Use tagreadbin in gstreamer extraction])
fi

##################################################################
# Check for tracker-extract: libxml2 for XML/HTML extractor
##################################################################

AC_ARG_ENABLE(libxml2,
              AS_HELP_STRING([--disable-libxml2],
                             [disable HTML/XML extractors [[default=yes]]]),,
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
      AC_MSG_ERROR([Couldn't find libxml2 >= $LIBXML2_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBXML2, test "x$have_libxml2" = "xyes")

##################################################################
# Check for tracker-extract: GdkPixbuf for MP3 album art
##################################################################

AC_ARG_ENABLE(gdkpixbuf,
              AS_HELP_STRING([--enable-gdkpixbuf],
                             [enable GdkPixbuf support, (MP3 album art) [[default=auto]]]),,
              [enable_gdkpixbuf=auto])

##################################################################
# Check for tracker-extract: Qt for MP3 album art
##################################################################

AC_ARG_ENABLE(qt,
              AS_HELP_STRING([--enable-qt],
                             [enable Qt support, (MP3 album art) [[default=auto]]]),,
              [enable_qt=auto])

selected_for_albumart="no  (disabled)"

##################################################################
# Check for tracker-extract: Choose between GdkPixbuf/Qt
##################################################################

if test "x$enable_qt" != "xno" && test "x$enable_gdkpixbuf" != "xyes"; then
   PKG_CHECK_MODULES(QT,
                     [QtGui >= $QT_REQUIRED],
                     [have_qt=yes],
                     [have_qt=no])

   TRACKER_EXTRACT_CFLAGS="$TRACKER_EXTRACT_CFLAGS $QT_CFLAGS"
   TRACKER_EXTRACT_LIBS="$TRACKER_EXTRACT_LIBS $QT_LIBS"

   if test "x$have_qt" = "xyes"; then
      AC_DEFINE(HAVE_QT, [], [Define if we have Qt])
      selected_for_albumart="yes (qt)"
   fi
else
   have_qt="no  (disabled)"
fi

if test "x$enable_qt" = "xyes"; then
   if test "x$have_qt" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Qt >= $QT_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_QT, test "x$have_qt" = "xyes")

if test "x$enable_gdkpixbuf" != "xno" && test "x$enable_quill" != "xyes"; then
   PKG_CHECK_MODULES(GDKPIXBUF,
                     [gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED],
                     [have_gdkpixbuf=yes],
                     [have_gdkpixbuf=no])

   TRACKER_EXTRACT_CFLAGS="$TRACKER_EXTRACT_CFLAGS $GDKPIXBUF_CFLAGS"
   TRACKER_EXTRACT_LIBS="$TRACKER_EXTRACT_LIBS $GDKPIXBUF_LIBS"

   if test "x$have_gdkpixbuf" = "xyes"; then
      AC_DEFINE(HAVE_GDKPIXBUF, [], [Define if we have GdkPixbuf])
      selected_for_albumart="yes (GdkPixbuf)"
   fi
else
   have_gdkpixbuf="no  (disabled)"
fi

if test "x$enable_gdkpixbuf" = "xyes"; then
   if test "x$have_gdkpixbuf" != "xyes"; then
      AC_MSG_ERROR([Couldn't find GdkPixbuf >= $GDKPIXBUF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_GDKPIXBUF, test "x$have_gdkpixbuf" = "xyes")

####################################################################
# Check for tracker-extract: gstreamer/xine/totem/mplayer/etc
####################################################################

AC_ARG_ENABLE(video-extractor,
              AS_HELP_STRING([--enable-video-extractor=ARG],
                             [enables one of the (gupnp-dlna, gstreamer, xine, external, auto) video extractor backends [[default=auto]]]),,
              [enable_video_extractor=auto])

PKG_CHECK_MODULES(GUPNP_DLNA,
                  [gupnp-dlna-1.0 >= 0.5],
                  [have_gupnp_dlna=yes],
                  [have_gupnp_dlna=no])

AC_SUBST(GUPNP_DLNA_CFLAGS)
AC_SUBST(GUPNP_DLNA_LIBS)

PKG_CHECK_MODULES(GSTREAMER,
                  [gstreamer-0.10 >= 0.10.12 gstreamer-tag-0.10 >= 0.10.12],
                  [have_libgstreamer=yes],
                  [have_libgstreamer=no])

AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(XINE,
                  [libxine >= 1.0],
                  [have_libxine=yes],
                  [have_libxine=no])

AC_SUBST(XINE_CFLAGS)
AC_SUBST(XINE_LIBS)

if test "x$enable_video_extractor" = "xauto"; then
   if test "$have_gupnp_dlna" = "yes"; then
      have_video_handler_app="GUPnP-DLNA"
      have_video_handler="yes"
   elif test "$have_libgstreamer" = "yes"; then
      have_video_handler_app="GStreamer"
      have_video_handler="yes"
   elif test "$have_libxine" = "yes"; then
      have_video_handler_app="Xine"
      have_video_handler="yes"
   else
      have_video_handler="?"
      have_video_handler_app="An external video player will be called"
   fi
elif test "x$enable_video_extractor" = "xgupnp-dlna"; then
   if test "$have_gupnp_dlna" = "yes"; then
      have_video_handler_app="GUPnP-DLNA"
      have_video_handler="yes"
   else
      AC_MSG_ERROR([Couldn't find GUPnP-DLNA])
   fi
elif test "x$enable_video_extractor" = "xgstreamer"; then
   if test "$have_libgstreamer" = "yes"; then
      have_video_handler_app="GStreamer"
      have_video_handler="yes"
   else
      AC_MSG_ERROR([Couldn't find Gstreamer])
   fi
elif test "x$enable_video_extractor" = "xxine"; then
   if test "$have_libxine" = "yes"; then
      have_video_handler_app="Xine"
      have_video_handler="yes"
   else
      AC_MSG_ERROR([Couldn't find libxine])
   fi
else
   have_video_handler="?"
   have_video_handler_app="An external video player will be called"
fi

if test "$have_video_handler_app" = "GUPnP-DLNA"; then
   AC_DEFINE(HAVE_GUPNP_DLNA, [], [Define if we have GUPnP-DLNA])
   AM_CONDITIONAL(HAVE_GUPNP_DLNA, true)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
elif test "$have_video_handler_app" = "GStreamer"; then
   AC_DEFINE(HAVE_GSTREAMER, [], [Define if we have GStreamer])
   AM_CONDITIONAL(HAVE_GSTREAMER, true)
   AM_CONDITIONAL(HAVE_GUPNP_DLNA, false)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
elif test "$have_video_handler_app" = "Xine"; then
   AC_DEFINE(HAVE_LIBXINE, [], [Define if we have Libxine])
   AM_CONDITIONAL(HAVE_LIBXINE, true)
   AM_CONDITIONAL(HAVE_GUPNP_DLNA, false)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
else
   AC_DEFINE(USING_EXTERNAL_VIDEO_PLAYER, [], [Define that Tracker will try to use external video players])
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, true)
   AM_CONDITIONAL(HAVE_GUPNP_DLNA, false)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
fi

##################################################################
# Check for tracker-extract: helixbin for gstreamer
##################################################################

AC_ARG_ENABLE(gstreamer-helix,
              AS_HELP_STRING([--enable-gstreamer-helix],
                             [enable GStreamer helix extractor for Real Media [[default=no]]]),,
              [enable_gstreamer_helix=no])

if test "x$enable_gstreamer_helix" != "xno"; then
   if test "x$have_libgstreamer" = "xyes"; then
      have_gstreamer_helix="yes"
   else
      have_gstreamer_helix="no"
   fi
else
   have_gstreamer_helix="no  (disabled)"
fi

if test "x$enable_gstreamer_helix" = "xyes"; then
   if test "x$have_gstreamer_helix" != "xyes"; then
      AC_MSG_ERROR([GStreamer is not available or disabled but GStreamer Helix Bin requested])
   fi
fi

AM_CONDITIONAL(HAVE_GSTREAMER_HELIX, test "x$have_gstreamer_helix" = "xyes")

####################################################################
# Check for tracker-extract: support .ps.gz files
####################################################################

AC_ARG_ENABLE([unzip_psgz_files],
              AS_HELP_STRING([--disable-unzip-ps-gz-files],
                             [disable unzipping ps.gz files [[default=yes]]]),,
              [enable_unzip_psgz_files=yes])

if test x$enable_unzip_psgz_files != "xno"; then
  AC_DEFINE(USING_UNZIPPSFILES, 1, [Unzip ps.gz files])
fi

##################################################################
# Check for tracker-extract: poppler for PDF
##################################################################

AC_ARG_ENABLE(poppler,
              AS_HELP_STRING([--enable-poppler],
                             [enable extractor for PDF data [[default=auto]]]),,
              [enable_poppler=auto])

if test "x$enable_poppler" != "xno" ; then
   PKG_CHECK_MODULES(POPPLER,
                     [poppler >= $POPPLER_REQUIRED],
                     [have_poppler=yes],
                     [have_poppler=no])

   AC_SUBST(POPPLER_CFLAGS)
   AC_SUBST(POPPLER_LIBS)

   if test "x$have_poppler" = "xyes"; then
      AC_DEFINE(HAVE_POPPLER, [], [Define if we have poppler])
   fi
else
   have_poppler="no  (disabled)"
fi

if test "x$enable_poppler" = "xyes"; then
   if test "x$have_poppler" != "xyes"; then
      AC_MSG_ERROR([Couldn't find poppler >= $POPPLER_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_POPPLER, test "x$have_poppler" = "xyes")

##################################################################
# Check for tracker-extract: libgsf
##################################################################

AC_ARG_ENABLE(libgsf,
              AS_HELP_STRING([--enable-libgsf],
                             [enable extractor for GSF data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libgsf >= $LIBGSF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBGSF, test "x$have_libgsf" = "xyes")

##################################################################
# Check for tracker-extract: libgif
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LIBS=""

AC_ARG_ENABLE(libgif,
              AS_HELP_STRING([--enable-libgif],
                             [enable extractor for GIF data [[default=auto]]]),,
              [enable_libgif=auto])

if test "x$enable_libgif" != "xno" ; then
   AC_CHECK_HEADER(gif_lib.h,
   AC_CHECK_LIB(gif,DGifOpen))

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
      AC_MSG_ERROR([Couldn't find libgif >= $LIBGIF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBGIF, test "x$have_libgif" = "xyes")

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: libjpeg
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LIBS=""

AC_ARG_ENABLE(libjpeg,
              AS_HELP_STRING([--enable-libjpeg],
                             [enable extractor for JPEG data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libjpeg >= $LIBJPEG_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBJPEG, test "x$have_libjpeg" = "xyes")

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: libtiff
##################################################################

# FIXME: This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LIBS=""

AC_ARG_ENABLE(libtiff,
              AS_HELP_STRING([--enable-libtiff],
                             [enable extractor for TIFF data [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find libtiff >= $LIBTIFF_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBTIFF, test "x$have_libtiff" = "xyes")

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for tracker-extract: vorbis
##################################################################

AC_ARG_ENABLE(libvorbis,
              AS_HELP_STRING([--enable-libvorbis],
                             [enable extractor for vorbis data (ogg) [[default=no]]]),,
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
      AC_MSG_ERROR([Couldn't find libvorbis >= $LIBVORBIS_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBVORBIS, test "x$have_libvorbis" = "xyes")

####################################################################
# Check for tracker-extract: flac
####################################################################

AC_ARG_ENABLE(libflac,
              AS_HELP_STRING([--enable-libflac],
                             [enable extractor for flac data [[default=no]]]),,
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
      AC_MSG_ERROR([Couldn't find libflac >= $LIBFLAC_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_LIBFLAC, test "x$have_libflac" = "xyes")

####################################################################
# Check for tracker-extract, tracker-writeback: totem-pl-parser
####################################################################

AC_ARG_ENABLE(playlist,
              AS_HELP_STRING([--enable-playlist],
                             [enable playlist support [[default=auto]]]),,
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
      AC_MSG_ERROR([Couldn't find Totem PL parser.])
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
                            [enable guaranteed existence of some metadata [[default=no]]]),
                            [enable_guarantee_metadata=yes],
                            [enable_guarantee_metadata=no])

if test "x$enable_guarantee_metadata" != "xno"; then
  AC_DEFINE(GUARANTEE_METADATA, 1, [Guarantee existence of certain metadata])
fi

##################################################################
# Check for tracker-extract: enable mockup extractor module?
##################################################################

have_mockup="no  (disabled)"

AM_CONDITIONAL(HAVE_MOCKUP, test "x$have_mockup" = "xyes")

##################################################################
# Write generated files
##################################################################

AC_CONFIG_FILES([
	data/dbus/Makefile
	data/icons/16x16/Makefile
	data/icons/22x22/Makefile
	data/icons/24x24/Makefile
	data/icons/32x32/Makefile
	data/icons/48x48/Makefile
	data/icons/Makefile
	data/icons/scalable/Makefile
	data/languages/Makefile
	data/Makefile
	data/miners/Makefile
	data/ontologies/Makefile
	data/tracker-sparql.pc
	data/tracker-client.pc
	data/tracker-extract.pc
	data/tracker-miner.pc
	docs/Makefile
	docs/design/Makefile
	docs/manpages/Makefile
	docs/reference/Makefile
	docs/reference/libtracker-client/Makefile
	docs/reference/libtracker-client/version.xml
	docs/reference/libtracker-sparql/Makefile
	docs/reference/libtracker-sparql/version.xml
	docs/reference/libtracker-extract/Makefile
	docs/reference/libtracker-extract/version.xml
	docs/reference/libtracker-miner/Makefile
	docs/reference/libtracker-miner/version.xml
	docs/reference/ontology/Makefile
	docs/reference/ontology/version.xml
	docs/tools/Makefile
	Makefile
	po/Makefile.in
	src/gvdb/Makefile
	src/libstemmer/Makefile
	src/libtracker-common/Makefile
	src/libtracker-sparql/Makefile
	src/libtracker-bus/Makefile
	src/libtracker-direct/Makefile
	src/libtracker-data/Makefile
	src/libtracker-fts/Makefile
	src/libtracker-extract/Makefile
	src/libtracker-client/Makefile
	src/libtracker-miner/Makefile
	src/Makefile
	src/miners/Makefile
	src/miners/fs/Makefile
	src/miners/rss/Makefile
	src/miners/flickr/Makefile
	src/tracker-store/Makefile
	src/tracker-control/Makefile
	src/tracker-extract/Makefile
	src/tracker-preferences/Makefile
	src/tracker-preferences/tracker-preferences.desktop.in
	src/tracker-search-bar/Makefile
	src/tracker-explorer/Makefile
	src/tracker-needle/Makefile
	src/tracker-needle/tracker-needle.desktop.in
	src/tracker-utils/Makefile
	src/tracker-writeback/Makefile
	src/plugins/Makefile
	src/plugins/evolution/Makefile
	src/plugins/nautilus/Makefile
	src/vapi/Makefile
	tests/common/Makefile
	tests/libtracker-client/Makefile
	tests/libtracker-common/Makefile
	tests/libtracker-extract/Makefile
	tests/libtracker-data/Makefile
	tests/libtracker-data/aggregates/Makefile
	tests/libtracker-data/algebra/Makefile
	tests/libtracker-data/anon/Makefile
	tests/libtracker-data/ask/Makefile
	tests/libtracker-data/basic/Makefile
	tests/libtracker-data/bnode-coreference/Makefile
	tests/libtracker-data/bound/Makefile
	tests/libtracker-data/datetime/Makefile
	tests/libtracker-data/expr-ops/Makefile
	tests/libtracker-data/functions/Makefile
	tests/libtracker-data/graph/Makefile
	tests/libtracker-data/nie/Makefile
	tests/libtracker-data/nmo/Makefile
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
	tests/functional-tests/test-configurations/Makefile
	tests/functional-tests/test-configurations/miner-basic-ops/Makefile
	tests/functional-tests/test-configurations/writeback/Makefile
	tests/functional-tests/test-configurations/applications/Makefile
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
	utils/lyrics/Makefile
	utils/playlists/Makefile
	utils/services/Makefile
        utils/data-generators/Makefile
        utils/data-generators/cc/Makefile
	utils/mtp/Makefile
	utils/tracker-sql/Makefile
	utils/tracker-resdump/Makefile
	examples/Makefile
	examples/libtracker-extract/Makefile
	examples/libtracker-miner/Makefile
	examples/rss-reader/Makefile
	examples/class-signal/Makefile
	examples/async-connection/Makefile
])

AC_OUTPUT

echo "
Build Configuration:

	Prefix:                                 ${prefix}
	Source code location:                   ${srcdir}
	Compiler:                               ${CC}
	Compiler Warnings:                      $enable_compile_warnings

	Win32:                                  $native_win32

	Enable gtk doc (for documentation):     $enable_gtk_doc

	Enable functional tests:                $enable_functional_tests
	Enable unit tests:                      $have_unit_tests

	Support for ioprio:                     $tracker_cv_have_ioprio
	Support for HAL:                        $have_hal
	Support for UPower:                     $have_upower
	Support for Cyrillic languages (enca):  $have_enca
	Support for network status detection:   $have_network_manager
	Unicode support library:                $with_unicode_support

Applications:

	Build with SQLite FTS support:          $have_tracker_fts

	Build tracker-preferences:              $have_tracker_preferences
	Build tracker-explorer:                 $have_tracker_explorer
	Build tracker-needle:                   $have_tracker_needle
	Build tracker-search-bar:               $have_tracker_search_bar

Metadata Extractors:

	Support libstreamanalyzer:              $have_libstreamanalyzer
	Support PNG:                            yes
	Support PDF:                            $have_poppler
	Support GIF:                            $have_libgif (xmp: $have_exempi)
	Support JPEG:                           $have_libjpeg (xmp: $have_exempi, exif: $have_libexif, iptc: $have_libiptcdata)
	Support TIFF:                           $have_libtiff (xmp: $have_exempi, exif: yes, iptc: $have_libiptcdata)
	Support Vorbis (ogg/etc):               $have_libvorbis
	Support Flac:                           $have_libflac
	Support MS & Open Office:               $have_libgsf
	Support XML / HTML:                     $have_libxml2
	Support embedded / sidecar XMP:         $have_exempi
	Support video formats:                  $have_video_handler ($have_video_handler_app) (tagreadbin: $enable_tagreadbin)
	Support helix formats (RPM/RM/etc):     $have_gstreamer_helix
	Support MP3 album art:                  $selected_for_albumart
	Support playlists (w/ Totem):           $have_playlist

Data Miners:

	Applications:                           yes (MeeGo support: $have_meegotouch)
	RSS:                                    $have_tracker_miner_rss
	Evolution:                              $have_tracker_miner_evolution ($tracker_miner_evolution_install_dir)
	Flickr:                                 $have_tracker_miner_flickr

Plugins:

	Nautilus: (tagging widget)              $have_tracker_nautilus_extension ($tracker_nautilus_extension_install_dir)

Writeback:

	Audio files using Taglib:               $have_taglib
	XMP:                                    $have_exempi

Frameworks / Options:

	Support Maemo                           $enable_maemo
        Support Guaranteed Metadata             $enable_guarantee_metadata (e.g. guess nie:title from files)
"

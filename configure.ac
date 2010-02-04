# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# This file is part of Tracker.

AC_PREREQ(2.59)

# Making releases:
#   TRACKER_MICRO_VERSION += 1;
#   TRACKER_INTERFACE_AGE += 1;
#   TRACKER_BINARY_AGE += 1;
# if any functions have been added, set TRACKER_INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set TRACKER_BINARY_AGE and TRACKER_INTERFACE_AGE to 0.
m4_define([tracker_major_version], [0])
m4_define([tracker_minor_version], [6])
m4_define([tracker_micro_version], [96])
m4_define([tracker_interface_age], [0])
m4_define([tracker_binary_age],
          [m4_eval(100 * tracker_minor_version + tracker_micro_version)])
m4_define([tracker_version],
          [tracker_major_version.tracker_minor_version.tracker_micro_version])
# This is the X.Y used in -llibtracker-FOO-X.Y
m4_define([tracker_api_version], [0.6])

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
m4_define([tracker_binary_version], [0.6.96])

AC_INIT([tracker], [tracker_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=tracker],
	[tracker])

AC_CONFIG_SRCDIR([src/trackerd/tracker-main.c])

AM_INIT_AUTOMAKE([no-define tar-ustar])
AM_CONFIG_HEADER(config.h)

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

AC_SUBST(PACKAGE_URL, [http://www.tracker-project.org])

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
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

CFLAGS="$CFLAGS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h sitdlib.h string.h sys/time.h unistd.h linux/unistd.h])

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

# Check for defines we expect
AC_TRY_COMPILE(,
	[
#include "stdio.h"
printf ("%s", __FUNCTION__);
	],
	have_function=yes,
	have_function=no,
)

if test "x$have_function" = "xno" ; then
   AC_DEFINE(__FUNCTION__, 
   	     __func__, 
   	     [Defined for compilers not supporting __FUNCTION__])
fi

# Library required versions
DBUS_REQUIRED=0.60
GLIB_REQUIRED=2.16.0
PANGO_REQUIRED=1.0.0
GTK_REQUIRED=2.16.0
GLADE_REQUIRED=2.5
QDBM_REQUIRED=1.8
GMIME_REQUIRED=2.4.0
LIBXML2_REQUIRED=0.6
LIBNOTIFY_REQUIRED=0.4.3
HAL_REQUIRED=0.5
GDKPIXBUF_REQUIRED=2.12.0
UNAC_REQUIRED=1.0.0
POPPLER_GLIB_REQUIRED=0.4.5
CAIRO_REQUIRED=1.0
GDK_REQUIRED=1.0
LIBVORBIS_REQUIRED=0.22
LIBEXIF_REQUIRED=0.6
LIBGSF_REQUIRED=1.13
EXEMPI_REQUIRED=1.99.2
HILDON_THUMBNAIL_REQUIRED=3.0.10
EVO_REQUIRED=2.25.5
EDS_REQUIRED=2.25.5
LIBGNOME_DESKTOP_REQUIRED=2.9.91
LIBGNOME_REQUIRED=2.13.2
LIBGNOMEUI_REQUIRED=2.13.7

# Library Checks
PKG_CHECK_MODULES(GLIB2, [glib-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# Check for GThread 2.0
PKG_CHECK_MODULES(GTHREAD, [gthread-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

# Check for GObject 2.0
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

# Check for GModule 2.0
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

# Check for Pango
PKG_CHECK_MODULES(PANGO, [pango >= $PANGO_REQUIRED])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

# Check for GMime
PKG_CHECK_MODULES(GMIME, [gmime-2.4 >= $GMIME_REQUIRED])
AC_SUBST(GMIME_CFLAGS)
AC_SUBST(GMIME_LIBS)

# Check for Dbus 0.50 or higher
PKG_CHECK_MODULES(DBUS, [dbus-1 >= $DBUS_REQUIRED dbus-glib-1 >= $DBUS_REQUIRED])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# Check for libpng 1.2 or higher
PKG_CHECK_MODULES(LIBPNG, [libpng >= 1.2])
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBPNG_LIBS)

# Check for GConf
PKG_CHECK_MODULES(GCONF, [gconf-2.0 >= 2.2.0], have_gconf=yes, have_gconf=no)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

AM_CONDITIONAL(HAVE_GCONF, test "$have_gconf" = "yes")

# Check for enca, detects defect Russian or Cyrillic language specifics in mp3s
PKG_CHECK_MODULES(ENCA, [enca >= 1.9], have_enca=yes, have_enca=no)
AC_SUBST(ENCA_CFLAGS)
AC_SUBST(ENCA_LIBS)

AM_CONDITIONAL(HAVE_ENCA, test "$have_enca" = "yes")

if test x$have_enca == "xyes"; then
  AC_DEFINE(HAVE_ENCA, 1, [Enca language detection aid])
fi

# Check for Raptor
PKG_CHECK_MODULES(RAPTOR, [raptor >= 1.4.17], have_raptor=yes, have_raptor=no)
AC_SUBST(RAPTOR_CFLAGS)
AC_SUBST(RAPTOR_LIBS)

if test x$have_raptor == "xyes"; then
  AC_DEFINE(HAVE_RAPTOR, 1, [Raptor RDF parsers])
fi

# Check we have the DBUS binding tool we need
AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
if test -z $DBUSBINDINGTOOL; then
   AC_MSG_ERROR([Could not find 'dbus-binding-tool'])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

####################################################################
# DBus Service
####################################################################

dnl DBus services dir
AC_ARG_WITH([session_bus_services_dir],
            AS_HELP_STRING([--with-session-bus-services-dir], 
	                   [Path to DBus services directory]))

if test "x$with_session_bus_services_dir" = "x" ; then
   services_dir="`pkg-config --variable session_bus_services_dir dbus-1`"
else
   services_dir="$with_session_bus_services_dir"
fi

DBUS_SERVICES_DIR="$services_dir"
AC_SUBST(DBUS_SERVICES_DIR)

####################################################################
# gettext/intltool support
####################################################################

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The gettext translation domain])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

####################################################################
# Enable GCOV coverage reporting support
####################################################################

AC_ARG_ENABLE(gcov,
              AS_HELP_STRING([--enable-gcov],
                             [enable coverage reporting support [[default=no]]]), ,
              [enable_gcov=no])

if test "x$enable_gcov" != "xno" ; then
  GCOV_CFLAGS="-ftest-coverage -fprofile-arcs"
  GCOV_LIBS="-lgcov"
else
  GCOV_CFLAGS=
  GCOV_LIBS=
fi

AC_SUBST(GCOV_CFLAGS)
AC_SUBST(GCOV_LIBS)

####################################################################
# Should we build GLib based unit tests
####################################################################

AC_ARG_ENABLE(unit_tests,
	      AS_HELP_STRING([--enable-unit-tests],
			     [enable unit tests [[default=no]]]), ,
	      [enable_unit_tests=no])

if test "x$enable_unit_tests" != "xno" ; then
   have_unit_tests=yes

   AC_DEFINE(HAVE_UNIT_TEST, 1, [Define if we have GLib unit test framework])

   AC_PATH_PROG(sqlite_exec, sqlite3)
   if test -z $sqlite_exec; then
      AC_MSG_ERROR([Could not find 'sqlite3'])
   fi

   AC_PATH_PROG(gtester, gtester)
   if test -z $gtester; then
      AC_MSG_ERROR([Could not find 'gtester'])
   fi
else
   have_unit_tests=no
fi

AM_CONDITIONAL(HAVE_UNIT_TESTS, test "x$have_unit_tests" = "xyes")

####################################################################
# QDBM check
####################################################################

AC_ARG_ENABLE(external-qdbm, 
	      AS_HELP_STRING([--enable-external-qdbm],
			     [enable external QDBM, not our imported version [[default=no]]]),,
              [enable_external_qdbm=no])

if test "x$enable_external_qdbm" != "xno"; then
   PKG_CHECK_MODULES(QDBM, 
   		     [qdbm >= $QDBM_REQUIRED],
		     [have_external_qdbm=yes],
		     [have_external_qdbm=no])

   if test "x$have_external_qdbm" = "xyes"; then
      AC_DEFINE(HAVE_EXTERNAL_QDBM, [], [Define if we have external QDBM])
   fi
else
   QDBM_CFLAGS="-I\$(top_srcdir)/src/qdbm"
   QDBM_LIBS="\$(top_builddir)/src/qdbm/libqdbm-private.la"
   AC_DEFINE(HAVE_QDBM, [], [Define if we have QDBM (our imported version)])
fi

if test "x$enable_external_qdbm" = "xyes"; then
   if test "x$have_external_qdbm" != "xyes"; then
      AC_MSG_ERROR([Couldn't find QDBM >= $QDBM_REQUIRED.])
   fi
fi

AC_SUBST(QDBM_CFLAGS)
AC_SUBST(QDBM_LIBS)

AM_CONDITIONAL(HAVE_EXTERNAL_QDBM, test "x$have_external_qdbm" = "xyes")

####################################################################
# Detailed metadata?
####################################################################

AC_ARG_ENABLE(detailed-metadata, 
	      AS_HELP_STRING([--disable-detailed-metadata],
			     [disable detailed metadata [[default=no]]]),,
              [enable_detailed_metadata=yes])

if test "x$enable_detailed_metadata" != "xno"; then
  AC_DEFINE(ENABLE_DETAILED_METADATA, 1, [Define if we want detailed metadata in the database])
fi

####################################################################
# Use tagreadbin in gstreamer extraction?
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
# Check for HAL
##################################################################

AC_ARG_ENABLE(hal, 
	      AS_HELP_STRING([--disable-hal],
			     [disable HAL support for AC power detection [[default=yes]]]),,
	      [enable_hal=yes])

if test "x$enable_hal" != "xno"; then
   PKG_CHECK_MODULES(HAL, 
    	  	     [hal >= $HAL_REQUIRED hal-storage], 
		     [have_hal=yes] , 
		     [have_hal=no])
   AC_SUBST(HAL_CFLAGS)
   AC_SUBST(HAL_LIBS)

   if test "x$have_hal" = "xyes"; then
      AC_DEFINE(HAVE_HAL, [], [Define if we have HAL])
   fi
else
   have_hal="no  (disabled)"
fi

if test "x$enable_hal" = "xyes"; then
   if test "x$have_hal" != "xyes"; then
      AC_MSG_ERROR([Couldn't find hal >= $HAL_REQUIRED and hal-storage.])
   fi
fi

AM_CONDITIONAL(HAVE_HAL, test "x$have_hal" = "xyes")

##################################################################
# Check for libxml2
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
# Check for GdkPixbuf, needed for the MP3 album art extractor
##################################################################

AC_ARG_ENABLE(gdkpixbuf, 
	      AS_HELP_STRING([--enable-gdkpixbuf],
			     [enable GdkPixbuf support, (MP3 album art) [[default=auto]]]),,
	      [enable_gdkpixbuf=auto])

if test "x$enable_gdkpixbuf" != "xno"; then
   PKG_CHECK_MODULES(GDKPIXBUF,
		     [gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED],
		     [have_gdkpixbuf=yes],
		     [have_gdkpixbuf=no])
   AC_SUBST(GDKPIXBUF_LIBS)
   AC_SUBST(GDKPIXBUF_CFLAGS)

   if test "x$have_gdkpixbuf" = "xyes"; then
      AC_DEFINE(HAVE_GDKPIXBUF, [], [Define if we have GdkPixbuf])
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
# Check for GStreamer or Xine. Otherwise, call an external video
# player (Totem or MPlayer).
####################################################################

AC_ARG_ENABLE(video-extractor,
	      AS_HELP_STRING([--enable-video-extractor=ARG],
	                     [enables one of the (gstreamer, xine, external, auto) video extractor backends [[default=auto]]]),,
              [enable_video_extractor=auto])

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
   if test "$have_libgstreamer" = "yes"; then
      have_video_handler_app="GStreamer"
      have_video_handler="yes"
   elif test "$have_libxine" = "yes"; then
      have_video_handler_app="Xine"
      have_video_handler="yes"
   else
      have_video_handler="?"
      have_video_handler_app="An external video player will be called"
   fi
elif test "x$enable_video_extractor" = "xgstreamer"; then
   if test "$have_libgstreamer" = "yes"; then
      have_video_handler_app="GStreamer"
      have_video_handler="yes"
   else
      AC_MSG_ERROR([***Gstreamer requested but not found - exiting!])
   fi
elif test "x$enable_video_extractor" = "xxine"; then
   if test "$have_libxine" = "yes"; then
      have_video_handler_app="Xine"
      have_video_handler="yes"
   else
      AC_MSG_ERROR([***libxine requested but not found - exiting!])
   fi
else
   have_video_handler="?"
   have_video_handler_app="An external video player will be called"
fi

if test "$have_video_handler_app" = "GStreamer"; then
   AC_DEFINE(HAVE_GSTREAMER, [], [Define if we have GStreamer])
   AM_CONDITIONAL(HAVE_GSTREAMER, true)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
elif test "$have_video_handler_app" = "Xine"; then
   AC_DEFINE(HAVE_LIBXINE, [], [Define if we have Libxine])
   AM_CONDITIONAL(HAVE_LIBXINE, true)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
else
   AC_DEFINE(USING_EXTERNAL_VIDEO_PLAYER, [], [Define that Tracker will try to use external video players])
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, true)
   AM_CONDITIONAL(HAVE_GSTREAMER, false)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
fi

##################################################################
# Check for Helixbin for GStreamer
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
# Windows check
####################################################################

AC_MSG_CHECKING(for WIN32)
AC_TRY_COMPILE(,
	[
	#ifndef WIN32
	#error
	#endif
	],
	native_win32=yes; AC_MSG_RESULT(yes),
	native_win32=no; AC_MSG_RESULT(no),
)

AM_CONDITIONAL(OS_WIN32, test "$native_win32" = "yes")

if test "$native_win32" = "yes" ; then
   AC_DEFINE(OS_WIN32, 1, [Define if we are on win32])
fi

####################################################################
#  libinotify
####################################################################

have_inotify=no
AC_CHECK_HEADERS([sys/inotify.h],[have_inotify=yes])

AM_CONDITIONAL(HAVE_INOTIFY, test "$have_inotify" = "yes")

if test "$have_inotify" = "yes" ; then
   AC_DEFINE(HAVE_INOTIFY, 1, [Define if we have inotify])
fi

####################################################################
#  Full Text Search (FTS)
####################################################################

AC_ARG_ENABLE([fts],
              AS_HELP_STRING([--disable-fts], 
	                     [disable full text search support [[default=no]]]),,
              [enable_fts=yes])

AM_CONDITIONAL(ENABLE_FTS, test "$enable_fts" = "yes")

if test "$enable_fts" = "yes" ; then
   AC_DEFINE(ENABLE_FTS, 1, [Define if we enable FTS support])
fi

####################################################################
#  SQLite checks
####################################################################

SQLITE_REQUIRED=3.5

PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= $SQLITE_REQUIRED])

AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)

AC_ARG_ENABLE([sqlite_fts],
              AS_HELP_STRING([--enable-sqlite-fts], 
	                     [enable SQLite FTS support [[default=no]]]),,
              [enable_sqlite_fts=no])

if test "x$enable_sqlite_fts" = "xyes"; then
   if test "x$enable_fts" != "xyes"; then
      AC_MSG_ERROR([Couldn't enable SQLite FTS support, FTS has been disabled.])
   fi
fi

if test "x$enable_sqlite_fts" != "xyes" ; then
   enable_sqlite_fts="no  (disabled)"
else
   AC_CHECK_LIB([sqlite3], [sqlite3_enable_load_extension], [:],
     [AC_MSG_ERROR([SQLite must be configured with --enable-load-extension.])])

   AC_DEFINE(HAVE_SQLITE_FTS, [], [Define if we have SQLite FTS])
fi

AM_CONDITIONAL(ENABLE_SQLITE_FTS, test "$enable_sqlite_fts" = "yes")

##################################################################
# Enable UNAC support?
##################################################################

AC_ARG_ENABLE(unac, 
	      AS_HELP_STRING([--enable-unac],
			     [enable UNAC support, required for stripping accents [[default=auto]]]),,
	      [enable_unac=auto])

if test "x$enable_unac" != "xno"; then
   PKG_CHECK_MODULES(UNAC,
		     [unac >= $UNAC_REQUIRED],
		     [have_unac=yes],
		     [have_unac=no])
   AC_SUBST(UNAC_LIBS)
   AC_SUBST(UNAC_CFLAGS)

   if test "x$have_unac" = "xyes"; then
      AC_DEFINE(HAVE_UNAC, [], [Define if we have UNAC for accent stripping])
   fi
else
   have_unac="no  (disabled)"
fi

if test "x$enable_unac" = "xyes"; then
   if test "x$have_unac" != "xyes"; then
      AC_MSG_ERROR([Couldn't find UNAC >= $UNAC_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_UNAC, test "x$have_unac" = "xyes")

####################################################################
# Push modules
####################################################################

AC_ARG_ENABLE([evolution_push_module],
              AS_HELP_STRING([--enable-evolution-push-module],
                             [enable support for Evolution push mechanism [[default=no]]]),,
              [enable_evolution_push_module=no])

AM_CONDITIONAL(USING_EVOLUTION_PUSH_MODULE, test "x$enable_evolution_push_module" = "xyes")

AC_ARG_ENABLE([kmail_push_module],
              AS_HELP_STRING([--enable-kmail-push-module],
                             [enable support for KMail push mechanism [[default=no]]]),,
              [enable_kmail_push_module=no])

AM_CONDITIONAL(USING_KMAIL_PUSH_MODULE, test "x$enable_kmail_push_module" = "xyes")

AC_ARG_ENABLE([rss_push_module],
              AS_HELP_STRING([--enable-rss-push-module],
                             [enable support for RSS push mechanism [[default=no]]]),,
              [enable_rss_push_module=no])

AM_CONDITIONAL(USING_RSS_PUSH_MODULE, test "x$enable_rss_push_module" = "xyes")

####################################################################
# Deskbar Applet Handler/Module
####################################################################

AC_ARG_ENABLE([deskbar_applet],
              AS_HELP_STRING([--enable-deskbar-applet], 
	      		     [enable support for Deskbar applet [[default=auto]]]),,
              [enable_deskbar_applet=auto])

if test "x$enable_deskbar_applet" != "xno" ; then
   if pkg-config --atleast-version=2.19 deskbar-applet ; then
      have_deskbar_applet="module"
   elif pkg-config --atleast-version=2.16 deskbar-applet ; then
      have_deskbar_applet="handler"
   else
      have_deskbar_applet="no"
   fi
fi

if test "x$enable_deskbar_applet" = "xyes"; then
   if test "x$have_deskbar_applet" = "xno"; then
      AC_MSG_ERROR([Couldn't find deskbar-applet >= 2.16.])
   fi
fi

AM_CONDITIONAL(HAVE_DESKBAR_APPLET_HANDLER, test "x$have_deskbar_applet" = "xhandler")
AM_CONDITIONAL(HAVE_DESKBAR_APPLET_MODULE, test "x$have_deskbar_applet" = "xmodule")

AC_ARG_WITH([deskbar_applet_dir],
            AS_HELP_STRING([--with-deskbar-applet-dir], 
	    		   [Path to Deskbar handler/module directory [[default=pkgconfig]]]))

if test "x$have_deskbar_applet" = "xhandler"; then
   if test -z "x$with_deskbar_applet_dir"; then
      deskbar_applet_dir="`pkg-config --variable handlersdir deskbar-applet`"
   else
      deskbar_applet_dir="$with_deskbar_applet_dir"
   fi

   if test -z "$deskbar_applet_dir"; then
      deskbar_applet_dir="\$(libdir)/deskbar-applet/handlers"
   fi
fi

if test "x$have_deskbar_applet" = "xmodule"; then
   if test -z "$with_deskbar_applet_dir"; then
       deskbar_applet_dir="`pkg-config --variable modulesdir deskbar-applet`"
   else
       deskbar_applet_dir="$with_deskbar_applet_dir"
   fi

   if test -z "$deskbar_applet_dir"; then
      deskbar_applet_dir="\$(libdir)/deskbar-applet/modules-2.20-compatible"
   fi
fi

DESKBAR_APPLET_DIR="$deskbar_applet_dir"
AC_SUBST(DESKBAR_APPLET_DIR)

##################################################################
# Enable building libtracker-gtk?
##################################################################

AC_ARG_ENABLE([libtrackergtk], 
              AS_HELP_STRING([--enable-libtrackergtk], 
	      		     [enable libtrackergtk [[default=auto]]]),,
	      [enable_libtrackergtk=auto])

if test "x$enable_libtrackergtk" != "xno" ; then
   PKG_CHECK_MODULES(LIBTRACKERGTK, [
		     glib-2.0    >= $GLIB_REQUIRED
		     gtk+-2.0    >= $GTK_REQUIRED
        	     dbus-1      >= $DBUS_REQUIRED
        	     dbus-glib-1 >= $DBUS_REQUIRED],
		     [have_libtrackergtk=yes], 
   		     [have_libtrackergtk=no])

   AC_SUBST([LIBTRACKERGTK_CFLAGS])
   AC_SUBST([LIBTRACKERGTK_LIBS])
else
   have_libtrackergtk="no  (disabled)"
fi

if test "x$enable_libtrackergtk" = "xyes"; then
   if test "x$have_libtrackergtk" != "xyes"; then
      AC_MSG_ERROR([Couldn't find libtracker_gtk dependencies (glib-2.0 >= $GLIB_REQUIRED, gtk+-2.0 >= $GTK_REQUIRED, dbus-1 >= $DBUS_REQUIRED, dbus-glib-1 >= $DBUS_REQUIRED).])
   fi
fi

AM_CONDITIONAL(HAVE_LIBTRACKERGTK, test "$have_libtrackergtk" = "yes")

##################################################################
# Enable building tracker-applet notification icon?
##################################################################

AC_ARG_ENABLE([tracker-applet], 
	      AS_HELP_STRING([--enable-tracker-applet], 
	                     [enable tracker-applet [[default=auto]]]),,
	      [enable_tracker_applet=auto])

if test "x$enable_tracker_applet" != "xno" ; then
   PKG_CHECK_MODULES(TRACKER_APPLET, [
		     glib-2.0    >= $GLIB_REQUIRED
		     gtk+-2.0    >= $GTK_REQUIRED
        	     dbus-1      >= $DBUS_REQUIRED
        	     dbus-glib-1 >= $DBUS_REQUIRED
		     libnotify   >= $LIBNOTIFY_REQUIRED],
		     [have_tracker_applet=yes], 
   		     [have_tracker_applet=no])

   AC_SUBST([TRACKER_APPLET_CFLAGS])
   AC_SUBST([TRACKER_APPLET_LIBS])
else
   have_tracker_applet="no  (disabled)"
fi

if test "x$enable_tracker_applet" = "xyes"; then
   if test "x$have_tracker_applet" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-applet dependencies (glib-2.0 >= $GLIB_REQUIRED, gtk+-2.0 >= $GTK_REQUIRED, dbus-1 >= $DBUS_REQUIRED, dbus-glib-1 >= $DBUS_REQUIRED, libnotify >= $LIBNOTIFY_REQUIRED).])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_APPLET, test "$have_tracker_applet" = "yes")

##################################################################
# Check for GNOME/GTK dependencies to build tracker search tool
##################################################################

AC_ARG_ENABLE(tracker-search-tool, 
              AS_HELP_STRING([--enable-tracker-search-tool], 
	      		     [enable tracker-search-tool [[default=auto]]]),,
	      [enable_tracker_search_tool=auto])

if test "x$enable_tracker_search_tool" != "xno"; then
   PKG_CHECK_MODULES(GNOME_UTILS, 
		     [libgnome-2.0 >= $LIBGNOME_REQUIRED
		      libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED],
	             [have_gnome_libs=yes], 
		     [have_gnome_libs=no])

   AC_SUBST(GNOME_UTILS_CFLAGS)
   AC_SUBST(GNOME_UTILS_LIBS)

   PKG_CHECK_MODULES(GNOMEDESKTOP, 
  		     [gnome-desktop-2.0 >= $LIBGNOME_DESKTOP_REQUIRED],
        	     [have_gnome_desktop=yes], 
	 	     [have_gnome_desktop=no])

   AC_SUBST(GNOMEDESKTOP_CFLAGS)
   AC_SUBST(GNOMEDESKTOP_LIBS)

   if test "x$have_gnome_libs" != "xyes"; then
      tracker_search_tool_error="Couldn't find GNOME library dependencies (libgnome-2.0 >= $LIBGNOME_REQUIRED, libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED)."
      have_tracker_search_tool="no"
   elif test "x$have_gnome_desktop" != "xyes"; then
      tracker_search_tool_error="Couldn't find GNOME desktop dependencies (gnome-desktop-2.0 >= $LIBGNOME_DESKTOP_REQUIRED)."
      have_tracker_search_tool="no"
   elif test "x$have_libtrackergtk" != "xyes"; then
      tracker_search_tool_error="Not building tracker-search-tool because libtracker-gtk is disabled or missing dependencies."
      have_tracker_search_tool="no"
   else 
      have_tracker_search_tool="yes"
      GNOME_COMMON_INIT
   fi
else
   have_tracker_search_tool="no  (disabled)"
fi

if test "x$enable_tracker_search_tool" = "xyes"; then
   if test "x$have_tracker_search_tool" != "xyes"; then
      AC_MSG_ERROR([$tracker_search_tool_error])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_SEARCH_TOOL, test "$have_tracker_search_tool" = "yes")

##################################################################
# Checks for tracker-preferences
##################################################################

AC_ARG_ENABLE([tracker-preferences],
              AS_HELP_STRING([--enable-tracker-preferences], 
	                     [enable the tracker preferences dialog [[default=auto]]]),,
              [enable_tracker_preferences=auto])

if test "x$enable_tracker_preferences" != "xno" ; then
   PKG_CHECK_MODULES(GLIB2, 
   	             [glib-2.0 >= $GLIB_REQUIRED],
		     [have_tracker_preferences=yes], 
		     [have_tracker_preferences=no])

   AC_SUBST([GLIB2_CFLAGS])
   AC_SUBST([GLIB2_LIBS])

   if test "$have_tracker_preferences" = "yes"; then
      PKG_CHECK_MODULES(GTK2, 
      			[gtk+-2.0 >= $GTK_REQUIRED],
			[have_tracker_preferences=yes], 
			[have_tracker_preferences=no])

      AC_SUBST([GTK2_CFLAGS])
      AC_SUBST([GTK2_LIBS])
   fi

   if test "$have_tracker_preferences" = "yes"; then
      PKG_CHECK_MODULES(LIBGLADE,  
      		        [libglade-2.0 >= $GLADE_REQUIRED], 
			[have_tracker_preferences=yes], 
			[have_tracker_preferences=no])

      AC_SUBST([LIBGLADE_CFLAGS])
      AC_SUBST([LIBGLADE_LIBS])
   fi
else
   have_tracker_preferences="no  (disabled)"
fi

if test "x$enable_tracker_preferences" = "xyes"; then
   if test "x$have_tracker_preferences" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-preferences dependencies (GLib >= $GLIB_REQUIRED, GTK+ >= $GTK_REQUIRED, Libglade >= $GLADE_REQUIRED).])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_PREFERENCES, test "$have_tracker_preferences" = "yes")

####################################################################
# PS extractor option
####################################################################

AC_ARG_ENABLE([unzip_psgz_files],
              AS_HELP_STRING([--disable-unzip-ps-gz-files],
                             [disable unzipping ps.gz files [[default=yes]]]),,
              [enable_unzip_psgz_files=yes])

if test x$enable_unzip_psgz_files != "xno"; then
  AC_DEFINE(USING_UNZIPPSFILES, 1, [Unzip ps.gz files])
fi

##################################################################
# Check for poppler's glib bingings
##################################################################

AC_ARG_ENABLE(poppler-glib, 
	      AS_HELP_STRING([--enable-poppler-glib], 
	      		     [enable extractor for PDF data [[default=auto]]]),,
	      [enable_poppler_glib=auto])

if test "x$enable_poppler_glib" != "xno" ; then
   PKG_CHECK_MODULES(POPPLER_GLIB, 
   		     [poppler-glib >= $POPPLER_GLIB_REQUIRED
		      cairo >= $CAIRO_REQUIRED
		      gdk-2.0 >= $GDK_REQUIRED],
		     [have_poppler_glib=yes],
		     [have_poppler_glib=no])

   AC_SUBST(POPPLER_GLIB_CFLAGS)
   AC_SUBST(POPPLER_GLIB_LIBS)

   if test "x$have_poppler_glib" = "xyes"; then
      AC_DEFINE(HAVE_POPPLER_GLIB, [], [Define if we have poppler-glib])
   fi
else
   have_poppler_glib="no  (disabled)"
fi

if test "x$enable_poppler_glib" = "xyes"; then
   if test "x$have_poppler_glib" != "xyes"; then
      AC_MSG_ERROR([Couldn't find poppler-glib >= $POPPLER_GLIB_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_POPPLER_GLIB, test "x$have_poppler_glib" = "xyes")

##################################################################
# Check for libexif
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

   AC_SUBST(LIBEXIF_CFLAGS)
   AC_SUBST(LIBEXIF_LIBS)

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
# Check for libiptcdata
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

   AC_SUBST(LIBIPTCDATA_CFLAGS)
   AC_SUBST(LIBIPTCDATA_LIBS)

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
# Check for libgsf
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
# Check for libjpeg
##################################################################

# FIXME This should be package based. Unfortunately in several main
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
# Check for libtiff
##################################################################

# FIXME This should be package based. Unfortunately in several main
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
# Check for vorbis
##################################################################

AC_ARG_ENABLE(libvorbis, 
              AS_HELP_STRING([--enable-libvorbis], 
     	                     [enable extractor for vorbis data (ogg) [[default=no]]]),,
	      [enable_libvorbis=no])

if test "x$enable_libvorbis" != "xno" ; then
   PKG_CHECK_MODULES(LIBVORBIS,
		     [vorbis >= $LIBVORBIS_REQUIRED],
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
# Check ioprio support
####################################################################

AC_MSG_CHECKING([[ioprio support]])
have_ioprio=no

AC_RUN_IFELSE(
[AC_LANG_PROGRAM([[
	#include <stdlib.h>
	#include <errno.h>
	#include <sys/syscall.h>
	#include <unistd.h>
]], 
[[
        inline int ioprio_get (int which, int who) 
	{
	       return syscall (__NR_ioprio_get, which, who);
	}

	int main ()
	{
	       return ioprio_get (1, 0);
	}
]]
)],
[have_ioprio=yes],[])

if test "$have_ioprio" = "yes" ; then
   AC_DEFINE(HAVE_IOPRIO, 1, [Define if we have ioprio])
fi

AC_MSG_RESULT([$have_ioprio])

##################################################################
# Check for exempi
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

   AC_SUBST(EXEMPI_CFLAGS)
   AC_SUBST(EXEMPI_LIBS)

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
# Checking totem-pl-parser
####################################################################

AC_ARG_ENABLE(playlist, 
	      AS_HELP_STRING([--enable-playlist],
			     [enable playlist support [[default=auto]]]),,
	      [enable_playlist=auto])

if test "x$enable_playlist" != "xno" ; then
   PKG_CHECK_MODULES(TOTEM_PL_PARSER, 
    	  	     [totem-plparser], 
		     [have_playlist=yes], 
		     [have_playlist=no])

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

##################################################################
# Enable Evolution plugin support?
##################################################################

if test "x$enable_evolution_push_module" != "xno" ; then
   PKG_CHECK_MODULES(EVOLUTION_PLUGIN, [
   		     evolution-plugin >= $EVO_REQUIRED
		     evolution-data-server-1.2 >= $EDS_REQUIRED], 
		     have_evolution_plugin=yes, 
		     have_evolution_plugin=no)

   AC_SUBST(EVOLUTION_PLUGIN_CFLAGS)
   AC_SUBST(EVOLUTION_PLUGIN_LIBS)

   if test x$have_evolution_plugin == "xyes"; then
      EVOLUTION_PLUGIN_INSTALL_DIR=`$PKG_CONFIG evolution-plugin --variable=plugindir`
   else
      EVOLUTION_PLUGIN_INSTALL_DIR=/dev/null
   fi

   AC_SUBST(EVOLUTION_PLUGIN_INSTALL_DIR)

   if test "x$have_evolution_plugin" = "xyes"; then
      AC_DEFINE(HAVE_EVOLUTION_PLUGIN, 1, [Define if we have Evolution plugin ])
   fi
else
   have_evolution_plugin="no  (disabled)"
fi

if test "x$enable_evolution_plugin" = "xyes"; then
   if test "x$have_evolution_plugin" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Evolution plugin requirements (evolution-dev, evolution-data-server-dev).])
   fi
fi

AM_CONDITIONAL(HAVE_EVOLUTION_PLUGIN, test "$have_evolution_plugin" = "yes")

####################################################################
# Checks for gtk-doc and docbook-tools
####################################################################

GTK_DOC_CHECK([1.8])

AC_CHECK_PROG(DB2HTML, db2html, true, false)
AM_CONDITIONAL(HAVE_DOCBOOK, $DB2HTML)

##################################################################
# Check for older tracker project files which can cause problems
##################################################################

old_exec_message=""
old_data_message=""

AC_CHECK_FILE("${prefix}/bin/trackerd", old_exec_trackerd=yes,,)
AC_CHECK_FILE("${prefix}/bin/tracker-indexer", old_exec_tracker_indexer=yes,,)
AC_CHECK_FILE("${prefix}/bin/tracker-extract", old_exec_tracker_extract=yes,,)
AC_CHECK_FILE("${prefix}/bin/tracker-thumbnailer", old_exec_tracker_thumbnailer=yes,,)
AC_CHECK_FILE("${DBUS_SERVICES_DIR}/tracker.service", old_data_dbus_service=yes,)
AC_CHECK_FILE("${prefix}/share/tracker/tracker-introspect.xml", old_data_dbus_xml=yes,,)
AC_CHECK_FILE("${prefix}/share/tracker/sqlite-service-stored-procs.sql", old_data_stored_procs=yes,,)

if test "x$old_exec_trackerd" == "xyes" -o \
        "x$old_exec_tracker_indexer" == "xyes" -o \
        "x$old_exec_tracker_extract" == "xyes" -o \
        "x$old_exec_tracker_thumbnailer" == "xyes"; then
   old_exec_message="
        Old Tracker executable files were found in your path.
        (trackerd, tracker-indexer, tracker-thumbnailer, tracker-extract)"
   old_file_action="
	** These files will be removed as part of the installation **"
fi

if test "x$old_data_dbus_service" == "xyes" -o \
        "x$old_data_dbus_xml" == "xyes" -o \
        "x$old_data_stored_procs" == "xyes"; then
   old_data_message="
        Old Tracker data files were found in the prefix you are installing to."
   old_file_action="
	** These files will be removed as part of the installation **"
fi

AM_CONDITIONAL(OLD_EXEC_REMOVE_ALL, test -n "$old_exec_message")
AM_CONDITIONAL(OLD_DATA_REMOVE_ALL, test -n "$old_data_message")

##################################################################
# Write generated files
##################################################################

AC_CONFIG_FILES([
	data/db/Makefile
	data/dbus/Makefile
	data/icons/16x16/Makefile
	data/icons/22x22/Makefile
	data/icons/24x24/Makefile
	data/icons/32x32/Makefile
	data/icons/48x48/Makefile
	data/icons/Makefile
	data/icons/scalable/Makefile
	data/languages/Makefile
	data/libtracker-gtk.pc
	data/Makefile
	data/modules/Makefile
	data/services/Makefile
	data/tracker.pc
	data/tracker-module-1.0.pc
	docs/Makefile
	docs/design/Makefile
	docs/manpages/Makefile
	docs/reference/Makefile
	docs/reference/libtracker-common/Makefile
	docs/reference/libtracker-common/version.xml
	docs/reference/libtracker-module/Makefile
	docs/reference/libtracker-module/version.xml
	filters/application/Makefile
	filters/Makefile
	filters/text/Makefile
	Makefile
	po/Makefile.in
	python/deskbar-handler/Makefile
	python/Makefile
	src/libinotify/Makefile
	src/libstemmer/Makefile
	src/libtracker-common/Makefile
	src/libtracker-data/Makefile
	src/libtracker-db/Makefile
	src/libtracker-gtk/Makefile
	src/libtracker/Makefile
	src/Makefile
	src/qdbm/Makefile
	src/tracker-applet/Makefile
	src/tracker-applet/tracker-applet.desktop.in
	src/trackerd/Makefile
	src/tracker-extract/Makefile
	src/tracker-fts/Makefile
	src/tracker-indexer/Makefile
	src/tracker-indexer/modules/Makefile
	src/tracker-preferences/Makefile
	src/tracker-preferences/tracker-preferences.desktop.in
	src/tracker-search-tool/Makefile
	src/tracker-search-tool/tracker-search-tool.desktop.in
	src/tracker-utils/Makefile
	src/plugins/Makefile
	src/plugins/evolution/Makefile
	src/plugins/kmail/Makefile
	src/plugins/rss/Makefile
	tests/common/Makefile
	tests/libtracker-common/Makefile
	tests/libtracker-db/Makefile
	tests/libtracker-data/Makefile
	tests/Makefile
	tests/scripts/dummy_data_start.sh
	tests/scripts/dummy_data_stop.sh
	tests/scripts/Makefile
	tests/tracker-indexer/Makefile
	tests/tracker-extract/Makefile
	utils/Makefile
	utils/albumart/Makefile
	utils/mp3-genre-leading-uppercase/Makefile
	utils/qdbm/Makefile
	utils/sqlite/Makefile
	utils/tracker-fts/Makefile
])

AC_OUTPUT

echo "
Build Configuration:

	Prefix:					${prefix}
	Source code location:			${srcdir}
	Compiler:				${CC}
	Compiler Warnings:			$enable_compile_warnings

	Win32:					$native_win32

	Enable unit tests:			$have_unit_tests
	Enable unac accent stripper:	  	$have_unac
	Enable enca encoding detection:	  	$have_enca
	Enable full text search:               	$enable_fts
	Enable detailed metadata:              	$enable_detailed_metadata

	Support for file monitoring:           	gio (w/ libinotify: $have_inotify)
	Support for ioprio:			$have_ioprio
	Support for HAL:                    	$have_hal

Applications:

	Build with SQLite FTS support:		$enable_sqlite_fts

	Build deskbar-applet:			$have_deskbar_applet
	Build tracker-search-tool:	        $have_tracker_search_tool
	Build tracker-preferences:		$have_tracker_preferences
	Build tracker-applet:	                $have_tracker_applet
	Build libtracker-gtk:			$have_libtrackergtk

Metadata Extractors:

	Support PNG:				yes
	Support PDF:				$have_poppler_glib
	Support JPEG:				$have_libjpeg (xmp: $have_exempi, exif: $have_libexif, iptc: $have_libiptcdata)
	Support TIFF:				$have_libtiff (xmp: $have_exempi, exif: yes, iptc: $have_libiptcdata)
	Support Vorbis (ogg/etc):		$have_libvorbis
	Support MS & Open Office:	        $have_libgsf
	Support XML / HTML:		        $have_libxml2
	Support embedded / sidecar XMP:		$have_exempi
	Support video formats:			$have_video_handler ($have_video_handler_app) (tagreadbin: $enable_tagreadbin)
	Support helix formats (RPM/RM/etc):     $have_gstreamer_helix
	Support MP3 album art (w/ GdkPixbuf):	$have_gdkpixbuf
	Support playlists (w/ Totem):           $have_playlist

Plugins:

	Evolution extracter (data-pull):	yes (old)
	Evolution plugin    (data-push):	$have_evolution_plugin
	KMail plugin        (data-push):	$enable_kmail_push_module
	RSS plugin          (data-push):	$enable_rss_push_module

Warning:

        You must make sure SQLite is compiled with --enable-threadsafe

	$old_exec_message
	$old_data_message
	$old_file_action

"

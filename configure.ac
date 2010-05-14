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
m4_define([tracker_minor_version], [9])
m4_define([tracker_micro_version], [4])
m4_define([tracker_interface_age], [0])
m4_define([tracker_binary_age],
          [m4_eval(100 * tracker_minor_version + tracker_micro_version)])
m4_define([tracker_version],
          [tracker_major_version.tracker_minor_version.tracker_micro_version])
# This is the X.Y used in -llibtracker-FOO-X.Y
m4_define([tracker_api_version], [0.9])

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
m4_define([tracker_binary_version], [0.9.0])

AC_INIT([tracker], [tracker_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=tracker],
	[tracker])

AC_CONFIG_SRCDIR([src/tracker-store/tracker-main.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([no-define tar-ustar])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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
AM_PROG_CC_C_O()
AC_PROG_CXX
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
DBUS_REQUIRED=1.0
DBUS_GLIB_REQUIRED=0.78
GLIB_REQUIRED=2.20.0
PANGO_REQUIRED=1.0.0
GTK_REQUIRED=2.18.0
LIBXML2_REQUIRED=2.6
LIBNOTIFY_REQUIRED=0.4.3
HAL_REQUIRED=0.5
UPOWER_REQUIRED=0.9.0
GDKPIXBUF_REQUIRED=2.12.0
QUILL_REQUIRED=1.0.0
UNAC_REQUIRED=1.0.0
POPPLER_REQUIRED=0.12.2
CAIRO_REQUIRED=1.0
GDK_REQUIRED=1.0
LIBVORBIS_REQUIRED=0.22
LIBFLAC_REQUIRED=1.2.1
LIBEXIF_REQUIRED=0.6
LIBGSF_REQUIRED=1.13
EXEMPI_REQUIRED=2.1.0
HILDON_THUMBNAIL_REQUIRED=3.0.10
EVO_REQUIRED=2.25.5
EDS_REQUIRED=2.25.5
# Unlikely version for now, Nepomuk integration isn't finished in streamanalyzer atm
LIBSTREAMANALYZER_REQUIRED=0.7.0
GEE_REQUIRED=0.3
TAGLIB_REQUIRED=1.6
GNOME_KEYRING_REQUIRED=2.26
LIBGRSS_REQUIRED=0.3
REST_REQUIRED=0.6

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

# Check for dbus and dbus-glib or higher
PKG_CHECK_MODULES(DBUS, [dbus-1 >= $DBUS_REQUIRED dbus-glib-1 >= $DBUS_GLIB_REQUIRED])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# Check for libpng 1.2 or higher
PKG_CHECK_MODULES(LIBPNG, [libpng >= 1.2])
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBPNG_LIBS)

# Check for libuuid
PKG_CHECK_MODULES(UUID, [uuid])
AC_SUBST(UUID_CFLAGS)
AC_SUBST(UUID_LIBS)

# Check for enca, detects defect Russian or Cyrillic language specifics in mp3s
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

AC_PATH_PROG(VALAC, valac, valac)
AC_SUBST(VALAC)

# Check we have the DBUS binding tool we need
AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
if test -z $DBUSBINDINGTOOL; then
   AC_MSG_ERROR([Could not find 'dbus-binding-tool'])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

####################################################################
# Taglib for audio writeback support
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
	AC_SUBST(TAGLIB_CFLAGS)
	AC_SUBST(TAGLIB_LIBS)
else
	have_taglib="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_TAGLIB, test "x$have_taglib" = "xyes")

####################################################################
# Stream Analyzer
####################################################################

AC_ARG_ENABLE(libstreamanalyzer,
	      AS_HELP_STRING([--enable-libstreamanalyzer],
			     [enable libstreamananalyzer [[default=no]]]), ,
	      [enable_libstreamanalyzer=no])

if test "x$enable_libstreamanalyzer" != "xno"; then
   PKG_CHECK_MODULES(LIBSTREAMANALYZER,
                     [libstreamanalyzer >= $LIBSTREAMANALYZER_REQUIRED],
		     [have_libstreamanalyzer=yes],
		     [have_libstreamanalyzer=no])
   AC_SUBST(LIBSTREAMANALYZER_CFLAGS)
   AC_SUBST(LIBSTREAMANALYZER_LIBS)

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
# DBus Service
####################################################################

dnl DBus services dir
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



####################################################################
# Should we install Maemo specific ontologies
####################################################################

AC_ARG_ENABLE(maemo,
	      AS_HELP_STRING([--enable-maemo],
			     [enable maemo ontology [[default=no]]]), ,
	      [enable_maemo=no])

AM_CONDITIONAL(HAVE_MAEMO, test "x$enable_maemo" = "xyes")

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
# Check for UPower
##################################################################

AC_ARG_ENABLE(upower,
	      AS_HELP_STRING([--disable-upower],
			     [disable UPower support for AC power detection [[default=auto]]]),,
	      [enable_upower=auto])

if test "x$enable_upower" != "xno"; then
   PKG_CHECK_MODULES(UPOWER,
		     [upower-glib >= $UPOWER_REQUIRED],
		     [have_upower=yes] ,
		     [have_upower=no])
   AC_SUBST(UPOWER_CFLAGS)
   AC_SUBST(UPOWER_LIBS)

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
# Check for HAL
##################################################################

AC_ARG_ENABLE(hal,
	      AS_HELP_STRING([--disable-hal],
			     [disable HAL support for AC power detection [[default=auto]]]),,
	      [enable_hal=auto])

if test "x$have_upower" != "xyes" && test "x$enable_hal" != "xno"; then
   PKG_CHECK_MODULES(HAL,
                     [hal >= $HAL_REQUIRED],
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
   if test "x$have_upower" = "xyes"; then
      AC_MSG_ERROR([Only one of UPower and HAL can be used.])
   elif test "x$have_hal" != "xyes"; then
      AC_MSG_ERROR([Couldn't find hal >= $HAL_REQUIRED.])
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

##################################################################
# Check for QUILL needed for the MP3 album art extractor
##################################################################

AC_ARG_ENABLE(quill,
              AS_HELP_STRING([--enable-quill],
                             [enable QUILL support, (MP3 album art) [[default=auto]]]),,
              [enable_quill=auto])

selected_for_albumart="no  (disabled)"


##################################################################
# Check for QUILL needed for the MP3 album art extractor cont
##################################################################


if test "x$enable_quill" != "xno" && test "x$enable_gdkpixbuf" != "xyes"; then
   PKG_CHECK_MODULES(QUILL,
		     [quill >= $QUILL_REQUIRED],
		     [have_quill=yes],
		     [have_quill=no])
   AC_SUBST(QUILL_LIBS)
   AC_SUBST(QUILL_CFLAGS)

   if test "x$have_quill" = "xyes"; then
      AC_DEFINE(HAVE_QUILL, [], [Define if we have QUILL])
      selected_for_albumart="yes (libquill)"
   fi
else
   have_quill="no  (disabled)"
fi

if test "x$enable_quill" = "xyes"; then
   if test "x$have_quill" != "xyes"; then
      AC_MSG_ERROR([Couldn't find QUILL >= $QUILL_REQUIRED.])
   fi
fi

AM_CONDITIONAL(HAVE_QUILL, test "x$have_quill" = "xyes")

##################################################################
# Check for GdkPixbuf, needed for the MP3 album art extractor cont
##################################################################

if test "x$enable_gdkpixbuf" != "xno" && test "x$enable_quill" != "xyes"; then
   PKG_CHECK_MODULES(GDKPIXBUF,
		     [gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED],
		     [have_gdkpixbuf=yes],
		     [have_gdkpixbuf=no])
   AC_SUBST(GDKPIXBUF_LIBS)
   AC_SUBST(GDKPIXBUF_CFLAGS)

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

AC_ARG_ENABLE(libinotify,
	      AS_HELP_STRING([--enable-libinotify],
			     [enables libinotify for the filesystem monitoring [[default=yes]]]),,
	      [enable_libinotify=yes])

if test "x$enable_libinotify" != "xno"; then
   AC_CHECK_HEADERS([sys/inotify.h],[have_libinotify=yes],[have_libinotify=no])

   AM_CONDITIONAL(HAVE_LIBINOTIFY, test "$have_libinotify" = "yes")

   if test "$have_libinotify" = "yes" ; then
      AC_DEFINE(HAVE_LIBINOTIFY, 1, [Have libinotify])
   fi
else
   have_libinotify="no  (disabled)"
fi

AM_CONDITIONAL(HAVE_LIBINOTIFY, test "$have_libinotify" = "yes")

if test "$have_libinotify" = "yes" ; then
  have_file_monitor="libinotify"
else
  have_file_monitor="gio"
fi

####################################################################
#  SQLite check
####################################################################

# 3.6.11 for sqlite_backup API
# 3.6.16 to fix test failures
# 3.6.17 for shared cache mode with virtual tables

SQLITE_REQUIRED=3.6.17

PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= $SQLITE_REQUIRED])
AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)

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

##################################################################
# Enable Gnome Keyring support to store credentials (for web miners)
##################################################################

AC_ARG_ENABLE(gnome_keyring,
	      AS_HELP_STRING([--enable-gnome-keyring],
			     [enable Gnome Keyring support to store credentials [[default=auto]]]),,
	      [enable_gnome_keyring=auto])

if test "x$enable_gnome_keyring" != "xno"; then
	PKG_CHECK_MODULES(GNOME_KEYRING,
	                  [ gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED ],
	                  [have_gnome_keyring=yes],
	                  [have_gnome_keyring=no])
	AC_SUBST(GNOME_KEYRING_LIBS)
	AC_SUBST(GNOME_KEYRING_CFLAGS)

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
# Flickr miner
##################################################################

AC_ARG_ENABLE(miner_flickr,
              AS_HELP_STRING([--enable-miner-flickr],
                             [enable Flickr miner [[default=auto]]]),,
                             [enable_miner_flickr=auto])

if test "x$enable_miner_flickr" != "xno"; then
   PKG_CHECK_MODULES(MINER_FLICKR,
                     [rest-0.6 >= $REST_REQUIRED],
                     [have_miner_flickr=yes],
                     [have_miner_flickr=no])
   AC_SUBST(MINER_FLICKR_LIBS)
   AC_SUBST(MINER_FLICKR_CFLAGS)
fi

if test "x$enable_miner_flickr" = "xyes"; then
   if test "x$have_miner_flickr" != "xyes"; then
      AC_MSG_ERROR([Couldn't find rest-0.6 >= $REST_REQUIRED for Flickr miner.])
   fi
fi

AM_CONDITIONAL(HAVE_MINER_FLICKR, test "x$have_miner_flickr" = "xyes")

####################################################################
# Miner Evolution
####################################################################

evolution_plugins_dir="/dev/null"

AC_ARG_ENABLE([miner_evolution],
              AS_HELP_STRING([--enable-miner-evolution],
                             [enable Evolution email data miner [[default=auto]]]),,
              [enable_miner_evolution=auto])

if test "x$enable_miner_evolution" != "xno"; then
   PKG_CHECK_MODULES(EVOLUTION_PLUGIN, [
   		     evolution-plugin >= $EVO_REQUIRED
		     evolution-data-server-1.2 >= $EDS_REQUIRED],
		     have_miner_evolution=yes,
		     have_miner_evolution=no)

   PKG_CHECK_EXISTS([evolution-data-server-1.2 >= 2.29.1],
		    [AC_DEFINE(HAVE_EDS_2_29_1, 1, [Define if we have eds 2.29.1 or newer])])

   AC_SUBST(EVOLUTION_PLUGIN_CFLAGS)
   AC_SUBST(EVOLUTION_PLUGIN_LIBS)

   if test "x$have_miner_evolution" = "xyes"; then
      dnl Evolution plugins dir
      AC_ARG_WITH([evolution_plugins_dir],
                  AS_HELP_STRING([--with-evolution-plugins-dir],
	                         [path to Evolution plugins directory]))
      if test "x$with_evolution_plugins_dir" = "x" ; then
         evolution_plugins_dir=`$PKG_CONFIG evolution-plugin --variable=plugindir`
      else
         evolution_plugins_dir="$with_evolution_plugins_dir"
      fi
   fi
else
   have_miner_evolution="no  (disabled)"
fi

if test "x$enable_miner_evolution" = "xyes"; then
   if test "x$have_miner_evolution" != "xyes"; then
      AC_MSG_ERROR([Couldn't find evolution-dev and evolution-data-server-dev for Evolution miner.])
   fi
fi

EVOLUTION_PLUGIN_INSTALL_DIR=$evolution_plugins_dir
AC_SUBST(EVOLUTION_PLUGIN_INSTALL_DIR)

AM_CONDITIONAL(HAVE_MINER_EVOLUTION, test "x$have_miner_evolution" = "xyes")


####################################################################
# Miner Kmail
####################################################################

AC_ARG_ENABLE([miner_kmail],
              AS_HELP_STRING([--enable-miner-kmail],
                             [enable KMail email data miner [[default=yes]]]),,
              [enable_miner_kmail=yes])

if test "x$enable_miner_kmail" = "xyes"; then
   have_miner_kmail="yes"
fi

AM_CONDITIONAL(HAVE_MINER_KMAIL, test "x$enable_miner_kmail" = "xyes")


##################################################################
# Miner RSS
##################################################################

AC_ARG_ENABLE([miner_rss],
              AS_HELP_STRING([--enable-miner-rss],
                             [enable RSS data miner [[default=auto]]]),,
              [enable_miner_rss=auto])

if test "x$enable_miner_rss" != "xno" ; then
   PKG_CHECK_MODULES(LIBGRSS,
                     [libgrss-0 >= $LIBGRSS_REQUIRED],
                     [have_miner_rss=yes],
                     [have_miner_rss=no])

   AC_SUBST(MINER_RSS_CFLAGS)
   AC_SUBST(MINER_RSS_LIBS)
else
   enable_miner_rss="no  (disabled)"
fi

if test "x$enable_miner_rss" = "xyes"; then
   if test "x$have_miner_rss" != "xyes"; then
      AC_MSG_ERROR([Couldn't find libgrss >= $LIBGRSS_REQUIRED for RSS miner.])
   fi
fi

AM_CONDITIONAL(HAVE_MINER_RSS, test "x$have_miner_rss" = "xyes")

####################################################################
# Application and Vala requirements
####################################################################

APP_REQUIREMENTS="glib-2.0    >= $GLIB_REQUIRED
                  gthread-2.0 >= $GLIB_REQUIRED
                  gmodule-2.0 >= $GLIB_REQUIRED
                  gtk+-2.0    >= $GTK_REQUIRED
                  dbus-1      >= $DBUS_REQUIRED
                  dbus-glib-1 >= $DBUS_GLIB_REQUIRED"

APPLET_REQUIREMENTS="libpanelapplet-2.0"
VALA_REQUIREMENTS="gee-1.0 >= $GEE_REQUIRED"

PKG_CHECK_MODULES(TRACKER_APPS,
                  [ $APP_REQUIREMENTS ],
                  [have_app_requirements=yes],
                  [have_app_requirements=no])

AC_SUBST(TRACKER_APPS_CFLAGS)
AC_SUBST(TRACKER_APPS_LIBS)

PKG_CHECK_MODULES(TRACKER_APPLETS,
                  [ $APPLET_REQUIREMENTS ],
                  [have_applet_requirements=yes],
                  [have_applet_requirements=no])

AC_SUBST(TRACKER_APPLETS_CFLAGS)
AC_SUBST(TRACKER_APPLETS_LIBS)

PKG_CHECK_MODULES(TRACKER_VALA,
                  [ $VALA_REQUIREMENTS ],
                  [have_vala_requirements=yes],
                  [have_vala_requirements=no])

AC_SUBST(TRACKER_VALA_CFLAGS)
AC_SUBST(TRACKER_VALA_LIBS)

##################################################################
# Enable building tracker-status-icon?
##################################################################

AC_ARG_ENABLE([tracker-status-icon],
	      AS_HELP_STRING([--enable-tracker-status-icon],
	                     [enable tracker-status-icon [[default=auto]]]),,
	      [enable_tracker_status_icon=auto])

if test "x$enable_tracker_status_icon" != "xno" ; then
   if test "x$have_app_requirements" != "xyes"; then
      have_tracker_status_icon="no"
   else
      have_tracker_status_icon="yes"
   fi
else
   have_tracker_status_icon="no  (disabled)"
fi

if test "x$enable_tracker_status_icon" = "xyes"; then
   if test "x$have_tracker_status_icon" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-status-icon dependencies ($APP_REQUIREMENTS).])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_STATUS_ICON, test "$have_tracker_status_icon" = "yes")

##################################################################
# Enable building tracker-explorer?
##################################################################

AC_ARG_ENABLE([tracker-explorer],
	      AS_HELP_STRING([--enable-tracker-explorer],
	                     [enable tracker-explorer[[default=auto]]]),,
	      [enable_tracker_explorer=auto])

if test "x$enable_tracker_explorer" != "xno" ; then
   if test "x$have_app_requirements" != "xyes" -o \
           "x$have_vala_requirements" != "xyes"; then
      have_tracker_explorer="no"
   else
      have_tracker_explorer="yes"
   fi
else
   have_tracker_explorer="no  (disabled)"
fi

if test "x$enable_tracker_explorer" = "xyes"; then
   if test "x$have_tracker_explorer" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-explorer dependencies ($APP_REQUIREMENTS $VALA_REQUIREMENTS).])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_EXPLORER, test "$have_tracker_explorer" = "yes")

##################################################################
# Enable building tracker-search-bar?
##################################################################

AC_ARG_ENABLE([tracker-search-bar],
	      AS_HELP_STRING([--enable-tracker-search-bar],
	                     [enable tracker-search-bar[[default=auto]]]),,
	      [enable_tracker_search_bar=auto])

if test "x$enable_tracker_search_bar" != "xno" ; then
   if test "x$have_app_requirements" != "xyes" -o \
           "x$have_applet_requirements" != "xyes"; then
      have_tracker_search_bar="no"
   else
      have_tracker_search_bar="yes"
   fi
else
   have_tracker_search_bar="no  (disabled)"
fi

if test "x$enable_tracker_search_bar" = "xyes"; then
   if test "x$have_tracker_search_bar" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-search-bar dependencies ($APP_REQUIREMENTS $APPLET_REQUIREMENTS).])
   fi
fi

AM_CONDITIONAL(HAVE_TRACKER_SEARCH_BAR, test "$have_tracker_search_bar" = "yes")

##################################################################
# Check for GNOME/GTK dependencies to build tracker search tool
##################################################################

AC_ARG_ENABLE(tracker-search-tool,
              AS_HELP_STRING([--enable-tracker-search-tool],
	      		     [enable tracker-search-tool [[default=auto]]]),,
	      [enable_tracker_search_tool=auto])

if test "x$enable_tracker_search_tool" != "xno"; then
   if test "x$have_app_requirements" != "xyes" -o \
           "x$have_vala_requirements" != "xyes"; then
      have_tracker_search_tool="no"
   else
      have_tracker_search_tool="yes"
   fi
else
   have_tracker_search_tool="no  (disabled)"
fi

if test "x$enable_tracker_search_tool" = "xyes"; then
   if test "x$have_tracker_search_tool" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-search-tool dependencies ($APP_REQUIREMENTS $VALA_REQUIREMENTS).])
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
   if test "x$have_app_requirements" != "xyes"; then
      have_tracker_preferences="no"
   else
      have_tracker_preferences="yes"
   fi
else
   have_tracker_preferences="no  (disabled)"
fi

if test "x$enable_tracker_preferences" = "xyes"; then
   if test "x$have_tracker_preferences" != "xyes"; then
      AC_MSG_ERROR([Couldn't find tracker-preferences dependencies ($APP_REQUIREMENTS).])
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
# Check for poppler
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
# Check for libgif
##################################################################

# FIXME This should be package based. Unfortunately in several main
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
# Check for flac
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
# Check ioprio support
####################################################################

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
# Enable mockup extractor module?
##################################################################

have_mockup="no  (disabled)"

AM_CONDITIONAL(HAVE_MOCKUP, test "x$have_mockup" = "xyes")


####################################################################
# Enable Nautilus extension support?
####################################################################

AC_ARG_ENABLE([nautilus-extension],
              AS_HELP_STRING([--enable-nautilus-extension],
                             [enable the nautilus extension [[default=auto]]]),,
              [enable_nautilus_extension=auto])

if test "x$enable_nautilus_extension" != "xno"; then
   PKG_CHECK_MODULES([NAUTILUS_EXTENSION],
                     [libnautilus-extension],
                     [have_nautilus_extension=yes],
                     [have_nautilus_extension=no])

   AC_SUBST(NAUTILUS_EXTENSION_CFLAGS)
   AC_SUBST(NAUTILUS_EXTENSION_LIBS)

   if test "x$have_nautilus_extension" = "xyes"; then
      AC_ARG_WITH([nautilus-extensions-dir],
                  AS_HELP_STRING([--with-nautilus-extensions-dir],
                                 [path to Nautilus extensions directory]))

      if test "x$with_nautilus_extensions_dir" = "x"; then
         nautilus_extensions_dir=`$PKG_CONFIG --variable=extensiondir libnautilus-extension`
      else
         nautilus_extensions_dir="$with_nautilus_extensions_dir"
      fi

      NAUTILUS_EXTENSION_INSTALL_DIR="$nautilus_extensions_dir"
   else
      NAUTILUS_EXTENSION_INSTALL_DIR="/dev/null"
   fi

   AC_SUBST([NAUTILUS_EXTENSION_INSTALL_DIR])

   if test "x$have_nautilus_extension" = "xyes"; then
      AC_DEFINE(HAVE_NAUTILUS_EXTENSION, 1, [Define if we have Nautilus extension])
   fi
else
   have_nautilus_extension="no  (disabled)"
fi

if test "x$enable_nautilus_extension" = "xyes"; then
   if test "x$have_nautilus_extension" != "xyes"; then
      AC_MSG_ERROR([Couldn't find Nautilus extension requirements (libnautilus-extension-dev).])
   fi
fi

AM_CONDITIONAL(HAVE_NAUTILUS_EXTENSION, test "$have_nautilus_extension" = "yes")

####################################################################
# Include/Exclude functional tests
####################################################################
AC_ARG_ENABLE([functional-tests],
              AS_HELP_STRING([--enable-functional-tests],
	                     [enable functional tests [[default=yes]]]),,
	     [enable_functional_tests=yes])

if test x$enable_functional_tests != "xno"; then
  AC_DEFINE(DIST_FUNCTIONAL_TESTS, 1, [Include functional tests in the installation])
fi

AM_CONDITIONAL(DIST_FUNCTIONAL_TESTS, test "x$enable_functional_tests" != "xno")

####################################################################
# Checks for gtk-doc and docbook-tools
####################################################################

GTK_DOC_CHECK([1.8])

if test "x$enable_gtk_doc" != "xno"; then
   AC_DEFINE(HAVE_GTK_DOC, 1, [Define if we have gtk-doc (with gtk-doc)])

   # Check for graphviz if we are building gtk_doc
   AC_PATH_PROG(GRAPHVIZ_FDP, fdp)
   AC_SUBST(GRAPHVIZ_FDP)

   if test -z "$GRAPHVIZ_FDP"; then
      AC_MSG_ERROR([Couldn't find Graphviz's fdp tool (graphviz).])
   fi
fi

AM_CONDITIONAL(HAVE_GTK_DOC, test "$enable_gtk_doc" = "yes")
AM_CONDITIONAL(HAVE_GRAPHVIZ_FDP, test -n "$GRAPHVIZ_FDP")

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

if test "x$old_exec_trackerd" = "xyes" -o \
        "x$old_exec_tracker_indexer" = "xyes" -o \
        "x$old_exec_tracker_extract" = "xyes" -o \
        "x$old_exec_tracker_thumbnailer" = "xyes"; then
   old_exec_message="
        Old Tracker executable files were found in your path.
        (trackerd, tracker-indexer, tracker-thumbnailer, tracker-extract)"
   old_file_action="
	** These files will be removed as part of the installation **"
fi

if test "x$old_data_dbus_service" = "xyes" -o \
        "x$old_data_dbus_xml" = "xyes" -o \
        "x$old_data_stored_procs" = "xyes"; then
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
	data/Makefile
	data/miners/Makefile
	data/ontologies/Makefile
	data/tracker-client.pc
	data/tracker-extract.pc
	data/tracker-miner.pc
	docs/Makefile
	docs/design/Makefile
	docs/manpages/Makefile
	docs/reference/Makefile
	docs/reference/libtracker-client/Makefile
	docs/reference/libtracker-client/version.xml
	docs/reference/libtracker-common/Makefile
	docs/reference/libtracker-common/version.xml
	docs/reference/libtracker-extract/Makefile
	docs/reference/libtracker-extract/version.xml
	docs/reference/libtracker-miner/Makefile
	docs/reference/libtracker-miner/version.xml
	docs/reference/ontology/Makefile
	docs/reference/ontology/version.xml
	docs/tools/Makefile
	Makefile
	po/Makefile.in
	src/libinotify/Makefile
	src/libstemmer/Makefile
	src/libtracker-common/Makefile
	src/libtracker-data/Makefile
	src/libtracker-db/Makefile
	src/libtracker-fts/Makefile
	src/libtracker-extract/Makefile
	src/libtracker-client/Makefile
	src/libtracker-miner/Makefile
	src/Makefile
	src/miners/Makefile
	src/miners/fs/Makefile
	src/miners/rss/Makefile
	src/miners/flickr/Makefile
	src/tracker-status-icon/Makefile
	src/tracker-status-icon/tracker-status-icon.desktop.in
	src/tracker-store/Makefile
	src/tracker-control/Makefile
	src/tracker-extract/Makefile
	src/tracker-preferences/Makefile
	src/tracker-preferences/tracker-preferences.desktop.in
	src/tracker-search-bar/Makefile
	src/tracker-search-tool/Makefile
	src/tracker-search-tool/tracker-search-tool.desktop.in
	src/tracker-explorer/Makefile
	src/tracker-utils/Makefile
	src/tracker-writeback/Makefile
	src/plugins/Makefile
	src/plugins/evolution/Makefile
	src/plugins/kmail/Makefile
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
	tests/libtracker-db/Makefile
	tests/libtracker-fts/Makefile
	tests/libtracker-fts/limits/Makefile
	tests/libtracker-fts/prefix/Makefile
	tests/functional-tests/Makefile
	tests/functional-tests/data/Makefile
	tests/functional-tests/data/Music/Makefile
	tests/functional-tests/data/Images/Makefile
	tests/functional-tests/data/Video/Makefile
	tests/Makefile
	tests/tracker-miner-fs/Makefile
	tests/tracker-extract/Makefile
	tests/tracker-store/Makefile
	tests/tracker-writeback/Makefile
	utils/Makefile
	utils/gtk-sparql/Makefile
	utils/lyrics/Makefile
	utils/playlists/Makefile
	utils/services/Makefile
	examples/Makefile
	examples/libtracker-extract/Makefile
	examples/libtracker-miner/Makefile
	examples/rss-reader/Makefile
])

AC_OUTPUT

echo "
Build Configuration:

	Prefix:					${prefix}
	Source code location:			${srcdir}
	Compiler:				${CC}
	Compiler Warnings:			$enable_compile_warnings

	Win32:					$native_win32

        Enable gtk doc (for documentation):     $enable_gtk_doc

	Enable functional tests:		$enable_functional_tests
	Enable unit tests:			$have_unit_tests

	Support for ioprio:			$tracker_cv_have_ioprio
	Support for HAL:			$have_hal
	Support for UPower:			$have_upower
	Support for file monitoring:            $have_file_monitor
	Support for accent stripping (unac):  	$have_unac
	Support for Cyrillic languages (enca): 	$have_enca

Applications:

	Build with SQLite FTS support:		yes

	Build tracker-search-tool:	        $have_tracker_search_tool
	Build tracker-preferences:		$have_tracker_preferences
	Build tracker-status-icon:              $have_tracker_status_icon
	Build tracker-explorer:                 $have_tracker_explorer
	Build tracker-search-bar:               $have_tracker_search_bar

Metadata Extractors:

	Support libstreamanalyzer:		$have_libstreamanalyzer
	Support PNG:				yes
	Support PDF:				$have_poppler
	Support GIF:                            $have_libgif (xmp: $have_exempi)
	Support JPEG:				$have_libjpeg (xmp: $have_exempi, exif: $have_libexif, iptc: $have_libiptcdata)
	Support TIFF:				$have_libtiff (xmp: $have_exempi, exif: yes, iptc: $have_libiptcdata)
	Support Vorbis (ogg/etc):		$have_libvorbis
	Support Flac:                           $have_libflac
	Support MS & Open Office:	        $have_libgsf
	Support XML / HTML:		        $have_libxml2
	Support embedded / sidecar XMP:		$have_exempi
	Support video formats:			$have_video_handler ($have_video_handler_app) (tagreadbin: $enable_tagreadbin)
	Support helix formats (RPM/RM/etc):     $have_gstreamer_helix
	Support MP3 album art:                  $selected_for_albumart
	Support playlists (w/ Totem):           $have_playlist

Data Miners:

        RSS:                                    $have_miner_rss
        Evolution:                              $have_miner_evolution ($evolution_plugins_dir)
        KMail:                                  $have_miner_kmail
        Flickr:                                 $have_miner_flickr

Plugins:

        Nautilus: (tagging widget)              $have_nautilus_extension

Writeback:

	Audio files using Taglib:               $have_taglib
	XMP:                                    $have_exempi

Warning:

        You must make sure SQLite is compiled with --enable-threadsafe

	$old_exec_message
	$old_data_message
	$old_file_action


"

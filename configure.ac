# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# This file is part of Tracker.

AC_PREREQ(2.59)
AC_INIT([tracker],[0.6.6],[tracker-list@gnome.org])

AC_CONFIG_SRCDIR([src/trackerd/tracker-main.c])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_SUBST(PACKAGE_URL, [http://www.tracker-project.org])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
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

# Library Checks
DBUS_REQUIRED=0.60
GLIB_REQUIRED=2.16.0
PANGO_REQUIRED=1.0.0
GMIME_REQUIRED=2.1.0

PKG_CHECK_MODULES(GLIB2, [glib-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# Check for gthread 2.0
PKG_CHECK_MODULES(GTHREAD, [ gthread-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

# Check for gobject 2.0
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

# Check for gmodule 2.0
PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES(GIO, [gio-2.0 >= $GLIB_REQUIRED])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

# Check for pango
PKG_CHECK_MODULES(PANGO, [pango >= $PANGO_REQUIRED])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

# Check for GMime
PKG_CHECK_MODULES(GMIME, [gmime-2.0 >= $GMIME_REQUIRED])
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
# Deskbar Applet Handler/Module
####################################################################

AC_ARG_ENABLE([deskbar_applet],
              AS_HELP_STRING([--enable-deskbar-applet=ARG], 
	      		     [Enable support for deskbar applet (handler, module, auto)]),,
              [enable_deskbar_applet=auto])

if test "x$enable_deskbar_applet" = "xauto" ; then
   if pkg-config --atleast-version=2.19 deskbar-applet ; then
      enable_deskbar_applet="module"
   elif pkg-config --atleast-version=2.16 deskbar-applet ; then
      enable_deskbar_applet="handler"
   else
      enable_deskbar_applet="no"
   fi
fi

AM_CONDITIONAL(USING_DESKBAR_HANDLER, test "x$enable_deskbar_applet" = "xhandler")
AM_CONDITIONAL(USING_DESKBAR_MODULE, test "x$enable_deskbar_applet" = "xmodule")

AC_ARG_WITH([deskbar_applet_dir],
            AS_HELP_STRING([--with-deskbar-applet-dir], [Path to Deskbar handler/module directory]))

if test "x$enable_deskbar_applet" = "xhandler" ; then
   if test "x$with_deskbar_applet_dir" = "x" ; then
      handler_dir="`pkg-config --variable handlersdir deskbar-applet`"
   else
      handler_dir="$with_deskbar_applet_dir"
   fi

   if test "x$handler_dir" = "x" ; then
      handler_dir="\$(exec_prefix)/lib/deskbar-applet/handlers"
   fi
fi

DESKBAR_HANDLER_DIR="$handler_dir"
AC_SUBST(DESKBAR_HANDLER_DIR)

if test "x$enable_deskbar_applet" = "xmodule" ; then
   if test "x$with_deskbar_applet_dir" = "x" ; then
       module_dir="`pkg-config --variable modulesdir deskbar-applet`"
   else
       module_dir="$with_deskbar_applet_dir"
   fi

   if test "x$module_dir" = "x" ; then
      module_dir="\$(exec_prefix)/lib/deskbar-applet/modules-2.20-compatible"
   fi
fi

DESKBAR_MODULE_DIR="$module_dir"
AC_SUBST(DESKBAR_MODULE_DIR)

####################################################################
# Compiler warning checks
####################################################################

AC_ARG_ENABLE(warnings,
              AS_HELP_STRING([--disable-warnings], [disable GCC warnings]),,
              [enable_warnings=yes])

# Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
   if test "x$enable_warnings" = "xyes"; then
      CFLAGS="\
	-Wall \
	-Wunused \
	-Wchar-subscripts \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wpointer-arith \
	-Wsign-compare \
	$CFLAGS"
   fi
fi

####################################################################
# gettext/intltool support
####################################################################

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext translation domain])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

####################################################################
# Check if glib includes GTests framework
#
# If glib is installed in a non-standard location (like /opt/xxx):
# * PKG_CONFIG_PATH must be set correctly
# * glib binaries must be in the PATH (i.e PATH=$PATH:/opt/xxx/bin)
####################################################################

GLIB_WITH_UNIT_TESTING=2.15.0

have_xesam_glib=no

AC_ARG_ENABLE(unit_tests,
	      AS_HELP_STRING([--enable-unit-tests=@<:@no/yes/auto@:>@],
			     [Enable unit tests (if available)]), ,
	      [enable_unit_tests=auto])

if test "x$enable_unit_tests" != "xno" ; then
   glib_pkgconfig_tmp="glib-2.0 >= $GLIB_WITH_UNIT_TESTING"
   PKG_CHECK_MODULES(GLIB_UNIT_TEST, 
                     $glib_pkgconfig_tmp,  
                     [have_unit_tests=yes], 
                     [have_unit_tests=no])

   if test "x$have_unit_tests" = "xyes" ; then
      AC_DEFINE(HAVE_UNIT_TEST, 1, [Unit test framework available in glib])

      AC_PATH_PROG(sqlite_exec, sqlite3)
      if test -z $sqlite_exec; then
         AC_MSG_ERROR([Could not find 'sqlite3'])
      fi

      glib_pkgconfig=$glib_pkgconfig_tmp
      AC_PATH_PROG(GTESTER, [gtester], [no] )
      if test "x$GTESTER" = "xno" ; then
      	 AC_MSG_ERROR([*** Gtester is not in the path])
      else
	PKG_CHECK_MODULES(XESAM_GLIB, 
		xesam-glib, [have_xesam_glib=yes], [have_xesam_glib=no])
      fi
   fi
else
   have_unit_tests=no
   glib_pkgconfig=
fi

if test "x$enable_unit_tests" = "xyes"; then
   if test "x$have_unit_tests" != "xyes"; then
      AC_MSG_ERROR([Couldn't find unit test compatible glib.])
   fi
fi

AM_CONDITIONAL(HAVE_UNIT_TESTS, test "x$have_unit_tests" = "xyes")
AM_CONDITIONAL(HAVE_XESAM_GLIB, test "x$have_xesam_glib" = "xyes")

####################################################################
# External QDBM check
####################################################################

QDBM_REQUIRED=1.8

AC_ARG_ENABLE(external_qdbm, 
	      AS_HELP_STRING([--enable-external-qdbm],
			     [build using system's qdbm]),,
              [enable_external_qdbm=no])

if test "x$enable_external_qdbm" = "xyes"; then
   PKG_CHECK_MODULES(QDBM, [ qdbm >= $QDBM_REQUIRED ])
   AM_CONDITIONAL(USING_EXTERNAL_QDBM, true)
else
   QDBM_CFLAGS="-I\$(top_srcdir)/src/qdbm"
   QDBM_LIBS="\$(top_builddir)/src/qdbm/libqdbm-private.la"
   AM_CONDITIONAL(USING_EXTERNAL_QDBM, false)
fi

AC_SUBST(QDBM_CFLAGS)
AC_SUBST(QDBM_LIBS)

##################################################################
# Check for libxml2
##################################################################

LIBXML2_REQUIRED=0.6

AC_ARG_ENABLE(libxml2, 
	      AS_HELP_STRING([--disable-libxml2],
			     [Disable HTML/XML extractors (full-text will still be available)]),,
	      [enable_libxml2=yes])

if test "x$enable_libxml2" = "xyes"; then
   PKG_CHECK_MODULES(LIBXML2, 
	 	    [libxml-2.0 >= $LIBXML2_REQUIRED],
		    [have_libxml2=yes],
		    [have_libxml2=no])
   AC_SUBST(LIBXML2_CFLAGS)
   AC_SUBST(LIBXML2_LIBS)
else
   have_libxml2="no (disabled)"
fi

if test "x$have_libxml2" = "xyes"; then
   AC_DEFINE(HAVE_LIBXML2, [], [Define if we have libxml2])
fi

AM_CONDITIONAL(HAVE_LIBXML2, test "x$have_libxml2" = "xyes")

##################################################################
# Check for HAL
##################################################################

AC_ARG_ENABLE(hal, 
	      AS_HELP_STRING([--disable-hal],
			     [Disable HAL support for ac power detection]),,
	      [enable_hal=yes])

if test "x$enable_hal" = "xyes"; then
   PKG_CHECK_MODULES(HAL, 
    	  	     [hal >= 0.5 hal-storage], 
		     [have_hal=yes] , 
		     [have_hal=no])
   AC_SUBST(HAL_CFLAGS)
   AC_SUBST(HAL_LIBS)
else
   have_hal="no (disabled)"
fi

if test "x$have_hal" = "xyes"; then
   AC_DEFINE(HAVE_HAL, [], [Define if we have HAL])
fi

AM_CONDITIONAL(HAVE_HAL, test "x$have_hal" = "xyes")

####################################################################
# check for GStreamer or Xine. Otherwise, call an external video
# player (Totem or MPlayer).
####################################################################

AC_ARG_ENABLE(video-extractor,
	      AS_HELP_STRING([--enable-video-extractor=ARG],
	                     [enables one of the (gstreamer, xine, external, auto) video extractor backends]),,
              [enable_video_extractor=auto])

PKG_CHECK_MODULES(GSTREAMER,
		  [gstreamer-0.10 >= 0.10.0],
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
      videos_handler="GStreamer"
      videos_are_handled="yes"
   elif test "$have_libxine" = "yes"; then
      videos_handler="Xine"
      videos_are_handled="yes"
   else
      videos_are_handled="?"
      videos_handler="An external video player will be called"
   fi
elif test "x$enable_video_extractor" = "xgstreamer"; then
   if test "$have_libgstreamer" = "yes"; then
      videos_handler="GStreamer"
      videos_are_handled="yes"
   else
      AC_MSG_ERROR([***Gstreamer requested but not found - exiting!])
   fi
elif test "x$enable_video_extractor" = "xxine"; then
   if test "$have_libxine" = "yes"; then
      videos_handler="Xine"
      videos_are_handled="yes"
   else
      AC_MSG_ERROR([***libxine requested but not found - exiting!])
   fi
else
   videos_are_handled="?"
   videos_handler="An external video player will be called"
fi

if test "$videos_handler" = "GStreamer"; then
   AC_DEFINE(HAVE_GSTREAMER, [], [Define if we have GStreamer])
   AM_CONDITIONAL(HAVE_GSTREAMER, true)
   AM_CONDITIONAL(HAVE_LIBXINE, false)
   AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
elif test "$videos_handler" = "Xine"; then
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

AC_MSG_CHECKING(for inotify)
AC_TRY_COMPILE(,
	[
        #include <sys/inotify.h>

        inotify_init(void);
	],
	have_inotify=no; AC_MSG_RESULT(no),
	have_inotify=yes; AC_MSG_RESULT(yes),
)

AM_CONDITIONAL(HAVE_INOTIFY, test "$have_inotify" = "yes")

if test "$have_inotify" = "yes" ; then
   AC_DEFINE(HAVE_INOTIFY, 1, [Define if we have inotify])
fi

####################################################################
#  SQLite check
####################################################################

SQLITE_REQUIRED=3.4

PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= $SQLITE_REQUIRED])

AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)

##################################################################
# Enable SQLite FTS support?
##################################################################

AC_ARG_ENABLE([sqlite_fts],
              AS_HELP_STRING([--enable-sqlite-fts], [Enable SQLite FTS support]),,
              [enable_sqlite_fts=no])

if test "x$enable_sqlite_fts" == "xno" ; then
   enable_sqlite_fts="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_SQLITE_FTS, test "$enable_sqlite_fts" = "yes")

##################################################################
# Enable UNAC support?
##################################################################

AC_ARG_ENABLE([unac], 
              AS_HELP_STRING([--disable-unac], [Disable unac accent stripper support]),,
	      [enable_unac=yes])

if test "x$enable_unac" != "xno" ; then
   PKG_CHECK_MODULES(UNAC, 
		     [unac >= 1.0.0],
		     [enable_unac=yes],
		     [enable_unac=no])

   AC_SUBST([UNAC_CFLAGS])
   AC_SUBST([UNAC_LIBS])
else
   enable_unac="no (disabled)"
fi

if test "$enable_unac" = "yes"; then
   AC_DEFINE(HAVE_UNAC, [], [Define if we have unac lib])
fi

AM_CONDITIONAL(ENABLE_UNAC, test "$enable_unac" = "yes")

##################################################################
# Enable building libtracker-gtk?
##################################################################

LIBTRACKERGTK_GTK_REQUIRED=2.8.20

AC_ARG_ENABLE([libtrackergtk], 
              AS_HELP_STRING([--disable-libtrackergtk], [Disable libtrackergtk]),,
	      [enable_libtrackergtk=yes])

if test "x$enable_libtrackergtk" != "xno" ; then
   libtrackergtk_modules="\
        	glib-2.0    >=  $GLIB_REQUIRED, \
        	gtk+-2.0    >=  $LIBTRACKERGTK_GTK_REQUIRED, \
        	dbus-1      >=  $DBUS_REQUIRED, \
        	dbus-glib-1 >=  $DBUS_REQUIRED"

   PKG_CHECK_MODULES(LIBTRACKERGTK, 
		     [$libtrackergtk_modules],
		     [enable_libtrackergtk=yes], 
		     [enable_libtrackergtk=no])

   AC_SUBST([LIBTRACKERGTK_CFLAGS])
   AC_SUBST([LIBTRACKERGTK_LIBS])
else
   enable_libtrackergtk="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_LIBTRACKERGTK, test "$enable_libtrackergtk" = "yes")

##################################################################
# Enable building tracker-applet notification icon?
##################################################################

LIBNOTIFY_GTK2_REQUIRED=2.10
LIBNOTIFY_GLIB_REQUIRED=2.10
LIBNOTIFY_REQUIRED=0.4.3

AC_ARG_ENABLE([trackerapplet], 
	      AS_HELP_STRING([--disable-trackerapplet], [Disable trackerapplet]),,
	      [enable_trackerapplet=yes])

if test "x$enable_trackerapplet" != "xno" ; then
   trackerapplet_modules="\
        	glib-2.0    >=  $LIBNOTIFY_GLIB_REQUIRED, \
        	gtk+-2.0    >=  $LIBNOTIFY_GTK2_REQUIRED, \
        	dbus-1      >=  $DBUS_REQUIRED, \
        	dbus-glib-1 >=  $DBUS_REQUIRED, \
		libnotify   >=  $LIBNOTIFY_REQUIRED"

   PKG_CHECK_MODULES(TRACKERAPPLET, 
   		     [$trackerapplet_modules],
		     [enable_trackerapplet=yes], 
   		     [enable_trackerapplet=no])

   AC_SUBST([TRACKERAPPLET_CFLAGS])
   AC_SUBST([TRACKERAPPLET_LIBS])
else
   enable_trackerapplet="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_TRACKERAPPLET, test "$enable_trackerapplet" = "yes")

##################################################################
# Check for GNOME/GTK dependencies to build tracker search tool
##################################################################

GTK_REQUIRED=2.8.0
LIBGNOME_DESKTOP_REQUIRED=2.9.91
LIBGNOME_REQUIRED=2.13.2
LIBGNOMEUI_REQUIRED=2.13.7
GNOMEVFS_REQUIRED=2.8.4

AC_ARG_ENABLE(gui, 
              AS_HELP_STRING([--disable-gui], [Disable building of the tracker-search-tool]),,
	      [enable_gui=yes])

if test "x$enable_gui" = "xyes"; then
   PKG_CHECK_MODULES(GNOME_UTILS, 
		     [gtk+-2.0 >= $GTK_REQUIRED
	              libgnome-2.0 >= $LIBGNOME_REQUIRED
		      libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED],
	             [have_gnome=yes], 
		     [have_gnome=no])
   if test "$have_gnome" = "yes"; then
       PKG_CHECK_MODULES(GNOMEVFS, 
			 [gnome-vfs-2.0 >= $GNOMEVFS_REQUIRED
			  gnome-vfs-module-2.0 >= $GNOMEVFS_REQUIRED],
		         [have_gnome=yes], 
			 [have_gnome=no])
   fi

   if test "$have_gnome" = "yes"; then
      PKG_CHECK_MODULES(GNOMEDESKTOP, 
			[gnome-desktop-2.0 >= $LIBGNOME_DESKTOP_REQUIRED],
        		[have_gnome=yes], 
			[have_gnome=no])
   fi

   AC_SUBST(GNOME_UTILS_CFLAGS)
   AC_SUBST(GNOME_UTILS_LIBS)

   AC_SUBST(GNOMEVFS_CFLAGS)
   AC_SUBST(GNOMEVFS_LIBS)

   AC_SUBST(GNOMEDESKTOP_CFLAGS)
   AC_SUBST(GNOMEDESKTOP_LIBS)
else
   have_gnome="no (disabled)"
fi

# do not build if libtracker-gtk is not being built
if test "x$enable_libtrackergtk" != "xyes"; then
   have_gnome="no (disabled as libtracker-gtk is not being built)"
fi

AM_CONDITIONAL(HAVE_GNOME, test "$have_gnome" = "yes")
if test "$have_gnome" = "yes"; then
   GNOME_COMMON_INIT
fi

##################################################################
# Checks for tracker-preferences
##################################################################

PREFERENCES_GLADE_REQUIRED=2.5
PREFERENCES_GTK_REQUIRED=2.8.0

AC_ARG_ENABLE([preferences],
              AS_HELP_STRING([--disable-preferences], [Disable the preferences dialog]),,
              [enable_preferences=yes])

if test "x$enable_preferences" != "xno" ; then
   PKG_CHECK_MODULES(GLIB2, 
   	             [glib-2.0 >= $PREFERENCES_GLIB_REQUIRED],
		     [enable_preference=yes], 
		     [enable_preference=no])

   AC_SUBST([GLIB2_CFLAGS])
   AC_SUBST([GLIB2_LIBS])

   if test "$enable_preferences" = "yes"; then
      PKG_CHECK_MODULES(GTK2, 
      			[gtk+-2.0 >= $PREFERENCES_GTK_REQUIRED],
			[enable_preference=yes], 
			[enable_preference=no])

      AC_SUBST([GTK2_CFLAGS])
      AC_SUBST([GTK2_LIBS])
   fi

   if test "$enable_preferences" = "yes"; then
      PKG_CHECK_MODULES(LIBGLADE,  
      		        [libglade-2.0 >= $PREFERENCES_GLADE_REQUIRED], 
			[enable_preferences=yes], 
			[enable_preferences=no])

      AC_SUBST([LIBGLADE_CFLAGS])
      AC_SUBST([LIBGLADE_LIBS])
   fi
else
   enable_preferences="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_PREFERENCES, test "$enable_preferences" = "yes")

##################################################################
# Check for poppler's glib bingings
##################################################################

POPPLER_GLIB_REQUIRED=0.4.5
CAIRO_REQUIRED=1.0
GDK_REQUIRED=1.0

AC_ARG_ENABLE(pdf, 
	      AS_HELP_STRING([--disable-pdf], [Disable PDF data extractor]),,
	      [enable_pdf=yes])

if test "x$enable_pdf" = "xyes"; then
   PKG_CHECK_MODULES(POPPLER_GLIB, 
   		     [poppler-glib >= $POPPLER_GLIB_REQUIRED
		      cairo >= $CAIRO_REQUIRED
		      gdk-2.0 >= $GDK_REQUIRED],
		     [have_poppler=yes],
		     [have_poppler=no])

   AC_SUBST(POPPLER_GLIB_CFLAGS)
   AC_SUBST(POPPLER_GLIB_LIBS)
else
   have_poppler="no (disabled)"
fi

AM_CONDITIONAL(HAVE_POPPLER_GLIB, test "$have_poppler" = "yes")

if test "$have_poppler" = "yes" ; then
   AC_DEFINE(HAVE_POPPLER, 1, [Define if we have poppler])
fi

##################################################################
# Check for libexif
##################################################################

LIBEXIF_REQUIRED=0.6

AC_ARG_ENABLE(exif, 
              AS_HELP_STRING([--disable-exif],[Disable exif data extractor]),,
	      [enable_exif=yes])

if test "x$enable_exif" = "xyes"; then
   PKG_CHECK_MODULES(LIBEXIF, 
	             [libexif >= $LIBEXIF_REQUIRED],
		     [have_libexif=yes],
		     [have_libexif=no])

   AC_SUBST(LIBEXIF_CFLAGS)
   AC_SUBST(LIBEXIF_LIBS)
else
   have_libexif="no (disabled)"
fi

AM_CONDITIONAL(HAVE_LIBEXIF, test "$have_libexif" = "yes")

if test "$have_libexif" = "yes" ; then
   AC_DEFINE(HAVE_LIBEXIF, 1, [Define if we have libexif])
fi

##################################################################
# Check for libgsf
##################################################################

LIBGSF_REQUIRED=1.13

AC_ARG_ENABLE(gsf, 
              AS_HELP_STRING([--disable-gsf], [Disable GSF data extractor]),,
	      [enable_gsf=yes])

if test "x$enable_gsf" = "xyes"; then
   PKG_CHECK_MODULES(LIBGSF,
                     [libgsf-1 >= $LIBGSF_REQUIRED],
		     [have_libgsf=yes],
		     [have_libgsf=no])

   AC_SUBST(LIBGSF_CFLAGS)
   AC_SUBST(LIBGSF_LIBS)
else
   have_libgsf="no (disabled)"
fi

AM_CONDITIONAL(HAVE_LIBGSF, test "$have_libgsf" = "yes")

if test "$have_libgsf" = "yes" ; then
   AC_DEFINE(HAVE_LIBGSF, 1, [Define if we have libgsf])
fi

##################################################################
# Check for libjpeg
##################################################################

# FIXME This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LIBS=""

AC_ARG_ENABLE(jpeg, 
	      AS_HELP_STRING([--disable-jpeg], [Disable jpeg extractor]),,
	      [enable_jpeg=yes])

if test "x$enable_jpeg" = "xyes"; then
   AC_CHECK_HEADER(jpeglib.h,
   AC_CHECK_LIB(jpeg, jpeg_CreateCompress))

   have_libjpeg=${ac_cv_lib_jpeg_jpeg_CreateCompress:-no}

   LIBJPEG_CFLAGS="$CFLAGS"
   LIBJPEG_LIBS="$LIBS"

   AC_SUBST(LIBJPEG_CFLAGS)
   AC_SUBST(LIBJPEG_LIBS)
else
   have_libjpeg="no (disabled)"
fi

AM_CONDITIONAL(HAVE_LIBJPEG, test "$have_libjpeg" = "yes")

if test "$have_libjpeg" = "yes" ; then
   AC_DEFINE(HAVE_LIBJPEG, 1, [Define if we have libjpeg])
fi

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"

##################################################################
# Check for libtiff
##################################################################

#
# FIXME This should be package based. Unfortunately in several main
# distros, it is not.

OLD_CFLAGS="$CFLAGS"
OLD_LIBS="$LIBS"
CFLAGS=""
LIBS=""

AC_ARG_ENABLE(tiff, 
              AS_HELP_STRING([--disable-tiff], [Disable tiff extractor]),,
	      [enable_tiff=yes])

if test "x$enable_tiff" = "xyes"; then
   AC_CHECK_HEADER(tiff.h,
   AC_CHECK_LIB(tiff, TIFFOpen))

   have_libtiff=${ac_cv_lib_tiff_TIFFOpen:-no}

   LIBTIFF_CFLAGS="$CFLAGS"
   LIBTIFF_LIBS="$LIBS"

   AC_SUBST(LIBTIFF_CFLAGS)
   AC_SUBST(LIBTIFF_LIBS)
else
   have_libtiff="no (disabled)"
fi

AM_CONDITIONAL(HAVE_LIBTIFF, test "$have_libtiff" = "yes")

if test "$have_libtiff" = "yes" ; then
   AC_DEFINE(HAVE_LIBTIFF, 1, [Define if we have libtiff])
fi

CFLAGS="$OLD_CFLAGS"
LIBS="$OLD_LIBS"

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

EXEMPI_REQUIRED=1.99.2

AC_ARG_ENABLE(xmp, 
              AS_HELP_STRING([--disable-xmp], [Disable XMP extraction]),,
	      [enable_xmp=yes])

if test "x$enable_xmp" = "xyes"; then
   PKG_CHECK_MODULES(EXEMPI,
		     [exempi-2.0 >= $EXEMPI_REQUIRED],
		     [have_exempi=yes], 
		     [have_exempi=no])

   AC_SUBST(EXEMPI_CFLAGS)
   AC_SUBST(EXEMPI_LIBS)
else
   have_exempi="no (disabled)"
fi

AM_CONDITIONAL(HAVE_EXEMPI, test "$have_exempi" = "yes")

if test "$have_exempi" = "yes" ; then
   AC_DEFINE(HAVE_EXEMPI, 1, [Define if we have exempi])
fi

##################################################################
# Check for Imagemagick
##################################################################

AC_ARG_ENABLE(imagemagick, 
              AS_HELP_STRING([--disable-imagemagick], [Disable Imagemagick thumbnailer]),,
	      [enable_imagemagick=yes])

if test "x$enable_imagemagick" = "xyes"; then
   AC_CHECK_PROG(HAVE_IMAGEMAGICK,convert,"yes","no",)
   have_imagemagick="$HAVE_IMAGEMAGICK"
else
   have_imagemagick="no (disabled)"
fi

AM_CONDITIONAL(HAVE_IMAGEMAGICK, test "$have_imagemagick" = "yes")

if test "$have_imagemagick" = "yes" ; then
   AC_DEFINE(HAVE_IMAGEMAGICK, 1, [Define if we have imagemagick])
fi

##################################################################
# Check for Hildon-thumbnail
##################################################################

PKG_CHECK_MODULES(HILDON_THUMBNAIL, 
		  hildon-thumbnail,
		  [has_hildon_thumbnail=yes],
		  [has_hildon_thumbnail=no])

if test "x$has_hildon_thumbnail" = "xyes"; then
   AC_CHECK_PROG(HAVE_HILDON_THUMBNAIL,hildon-thumb-gdk-pixbuf,"yes","no",)
   have_hildon_thumbnail="$HAVE_HILDON_THUMBNAIL"
else
   HILDON_THUMBNAIL_CFLAGS=""
   HILDON_THUMBNAIL_LIBS=""
   have_hildon_thumbnail="no (disabled)"
fi

AM_CONDITIONAL(HAVE_HILDON_THUMBNAIL, test "$have_hildon_thumbnail" = "yes")

if test "$have_hildon_thumbnail" = "yes" ; then
   AC_DEFINE(HAVE_HILDON_THUMBNAIL, 1, [Define if we have hildon-thumbnail])
fi

AC_SUBST(HILDON_THUMBNAIL_CFLAGS)
AC_SUBST(HILDON_THUMBNAIL_LIBS)

AC_CONFIG_LINKS(tests/tracker-indexer/tracker-dbus.c:src/tracker-indexer/tracker-dbus.c
                tests/tracker-indexer/tracker-dbus.h:src/tracker-indexer/tracker-dbus.h
                tests/tracker-indexer/tracker-indexer.c:src/tracker-indexer/tracker-indexer.c
                tests/tracker-indexer/tracker-indexer.h:src/tracker-indexer/tracker-indexer.h
                tests/tracker-indexer/tracker-indexer-db.c:src/tracker-indexer/tracker-indexer-db.c
                tests/tracker-indexer/tracker-indexer-db.h:src/tracker-indexer/tracker-indexer-db.h
                tests/tracker-indexer/tracker-indexer-module.c:src/tracker-indexer/tracker-indexer-module.c
                tests/tracker-indexer/tracker-indexer-module.h:src/tracker-indexer/tracker-indexer-module.h
                tests/tracker-indexer/tracker-marshal-main.c:src/tracker-indexer/tracker-marshal-main.c
		tests/tracker-indexer/tracker-metadata.c:src/tracker-indexer/tracker-metadata.c 
                tests/tracker-indexer/tracker-metadata.h:src/tracker-indexer/tracker-metadata.h
                tests/tracker-indexer/tracker-metadata-utils.c:src/tracker-indexer/tracker-metadata-utils.c
                tests/tracker-indexer/tracker-metadata-utils.h:src/tracker-indexer/tracker-metadata-utils.h
                tests/tracker-indexer/tracker-module.h:src/tracker-indexer/tracker-module.h
)

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
	data/trackerd.desktop.in
	docs/Makefile
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
	src/tracker-thumbnailer/Makefile
	src/tracker-utils/Makefile
	tests/common/Makefile
	tests/libtracker-common/Makefile
	tests/libtracker-db/Makefile
	tests/Makefile
	tests/scripts/dummy_data_start.sh
	tests/scripts/dummy_data_stop.sh
	tests/scripts/Makefile
	tests/trackerd/Makefile
	tests/trackerd/xesam/Makefile
	tests/tracker-fts/Makefile
	tests/tracker-indexer/Makefile
	thumbnailers/application/Makefile
	thumbnailers/image/Makefile
	thumbnailers/Makefile
	utils/Makefile
	utils/qdbm/Makefile
])

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT

echo "
Build Configuration:

	Prefix:					${prefix}
	Source code location:			${srcdir}
	Compiler:				${CC}

	Win32:					$native_win32

	Enable warnings:			$enable_warnings
	Enable debug symbols:    		$enable_debug_code
	Enable unit tests:			$have_unit_tests
	Enable unac accent stripper:	  	$enable_unac

	Support for file monitoring:           	gio
	Support for ioprio:			$have_ioprio
	Support for HAL:                    	$have_hal

Applications:

	Build with SQLite FTS support:		$enable_sqlite_fts

	Build deskbar-applet:			$enable_deskbar_applet
	Build tracker-search-tool:	        $have_gnome
	Build tracker-preferences:		$enable_preferences
	Build tracker-applet:	                $enable_trackerapplet
	Build libtracker-gtk:			$enable_libtrackergtk

Metadata Extractors:

	Support png:				yes
	Support pdf:				$have_poppler
	Support jpeg:				$have_libjpeg (xmp: $have_exempi exif: $have_libexif)
	Support tiff:				$have_libtiff (xmp: $have_exempi exif: yes)
	Support ms & openoffice (gsf):	        $have_libgsf
	Support xml / html formats:		$have_libxml2
	Support embedded / sidecar xmp:		$have_exempi
	Support video files:			$videos_are_handled ($videos_handler)

Thumbnailers:

	Have Imagemagick			$have_imagemagick
	Have hildon-thumbnail			$have_hildon_thumbnail

Warning:

        You must make sure SQLite is compiled with --enable-threadsafe

	$old_exec_message
	$old_data_message
	$old_file_action

"

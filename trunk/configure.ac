# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# This file is part of Tracker.

AC_PREREQ(2.59c)
AC_INIT([tracker],[0.6.4],[tracker-list@gnome.org])

AC_CONFIG_SRCDIR([src/trackerd/trackerd.c])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_SUBST(PACKAGE_URL, [http://www.tracker-project.org])

CFLAGS="$CFLAGS"

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h sitdlib.h string.h sys/time.h unistd.h linux/unistd.h])

AC_CHECK_HEADER([zlib.h], [], [AC_MSG_ERROR([You must have zlib.h and zlib installed])])

dnl can posix_fadvise be used
AC_CHECK_DECLS(posix_fadvise, [], [], [
#define _XOPEN_SOURCE 600
#include <fcntl.h>])

# Checks for functions
AC_CHECK_FUNCS([posix_fadvise])
AC_CHECK_FUNCS([getline])

dnl Library Checks
DBUS_REQUIRED=0.60
GLIB_REQUIRED=2.10.0
PANGO_REQUIRED=1.0.0
GMIME_REQUIRED=2.1.0

PKG_CHECK_MODULES(GLIB2, [ glib-2.0 >= $GLIB_REQUIRED ])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

# Check for gthread 2.0
PKG_CHECK_MODULES(GTHREAD, [ gthread-2.0 >= $GLIB_REQUIRED ])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

# Check for gobject 2.0
PKG_CHECK_MODULES(GOBJECT, [ gobject-2.0 >= $GLIB_REQUIRED ])
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

#Check for pango
PKG_CHECK_MODULES(PANGO, [ pango >= $PANGO_REQUIRED ])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

# Check for GMime
PKG_CHECK_MODULES(GMIME, [ gmime-2.0 >= $GMIME_REQUIRED ])
AC_SUBST(GMIME_CFLAGS)
AC_SUBST(GMIME_LIBS)

# Check for Dbus 0.50 or higher
PKG_CHECK_MODULES(DBUS, [ dbus-1 >= $DBUS_REQUIRED dbus-glib-1 >= $DBUS_REQUIRED ])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# Check for libpng 1.2 or higher
PKG_CHECK_MODULES(LIBPNG, [ libpng >= 1.2 ])
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBPNG_LIBS)

####################################################################
# DBus Service
####################################################################

dnl DBus services dir
AC_ARG_WITH([session_bus_services_dir],
            AC_HELP_STRING([--with-session-bus-services-dir], [Path to DBus services directory]))

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
              AC_HELP_STRING([--enable-deskbar-applet=ARG], [Enable support for deskbar applet (handler, module, auto)]),,
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
            AC_HELP_STRING([--with-deskbar-applet-dir], [Path to Deskbar handler/module directory]))

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
              AC_HELP_STRING([--disable-warnings], [disable GCC warnings]),,
              [enable_warnings=yes])

dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test "x$enable_warnings" = "xyes"; then
     CFLAGS="\
     -Wall \
     -Wchar-subscripts \
     -Wnested-externs \
     -Wpointer-arith \
     -Wsign-compare \
     $CFLAGS"
  fi
fi

####################################################################
# Compiler generate debug code
####################################################################


AC_ARG_ENABLE([debug_code],
              AC_HELP_STRING([--enable-debug-code], [build with debug symbols]),,
              [enable_debug_code=no])

if test "x$enable_debug_code" = "xyes"; then
	CFLAGS="-g $CFLAGS"
fi

####################################################################
# gettext/intltool support 
####################################################################

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext translation domain])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

####################################################################
# check for newer glib (>= 2.12.0) for g_option_context_set_summary ()
# availability
# used as well in tracker-configuration.c and tracker-metadata-tile.c
# for other methods availability
####################################################################
RECENT_GLIB=2.12.0

PKG_CHECK_MODULES(glib2, [ glib-2.0 >= $RECENT_GLIB ],
                  [have_recent_glib=yes], [have_recent_glib=no])

if test "$have_recent_glib" = "yes" ; then
   AC_DEFINE(HAVE_RECENT_GLIB, 1, [Define if we have recent glib])
fi

AM_CONDITIONAL(HAVE_RECENT_GLIB, test "$have_recent_glib" = "yes")



####################################################################
# External QDBM check
####################################################################

QDBM_REQUIRED=1.8

AC_ARG_ENABLE(external_qdbm, 
	AC_HELP_STRING(
		[--enable-external-qdbm],
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
# check for libxml2
##################################################################
  
LIBXML2_REQUIRED=0.6

AC_ARG_ENABLE(libxml2, AC_HELP_STRING([--disable-libxml2],[Disable HTML/XML extractors (full-text will still be available)]),,[enable_libxml2=yes])
if test "x$enable_libxml2" = "xyes"; then
	PKG_CHECK_MODULES(LIBXML2, [
		libxml-2.0 >= $LIBXML2_REQUIRED],
		[have_libxml2=yes] , [have_libxml2=no])
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
# check for HAL
##################################################################
  
AC_ARG_ENABLE(hal, AC_HELP_STRING([--disable-hal],[Disable HAL support for ac power detection]),,[enable_hal=yes])
if test "x$enable_hal" = "xyes"; then
	PKG_CHECK_MODULES(HAL, [hal >= 0.5], [have_hal=yes] , [have_hal=no])
	AC_SUBST(HAL_CFLAGS)
	AC_SUBST(HAL_LIBS)
else
	have_hal="no (disabled)"
fi

if test "x$have_hal" = "xyes"; then
	AC_DEFINE(HAVE_HAL, [], [Define if we have HAL])
fi

####################################################################
# check for GStreamer or Xine. Otherwise, call an external video
# player (Totem or MPlayer).
####################################################################

AC_ARG_ENABLE(video-extractor,
	AC_HELP_STRING(
		[--enable-video-extractor=ARG],
		[enables one of the (gstreamer, xine, external, auto) video extractor backends]),,
	[enable_video_extractor=auto])

PKG_CHECK_MODULES(GSTREAMER, 
	[gstreamer-0.10 >= 0.10.0],
	[have_libgstreamer=yes] , [have_libgstreamer=no])
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(XINE, 
	[libxine >= 1.0],
	[have_libxine=yes] , [have_libxine=no])
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
	AM_CONDITIONAL(USING_GSTREAMER, true)
	AM_CONDITIONAL(USING_LIBXINE, false)
	AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
elif test "$videos_handler" = "Xine"; then
	AC_DEFINE(HAVE_LIBXINE, [], [Define if we have Libxine])
	AM_CONDITIONAL(USING_LIBXINE, true)
	AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, false)
	AM_CONDITIONAL(USING_GSTREAMER, false)
else
	AM_CONDITIONAL(USING_EXTERNAL_VIDEO_PLAYER, true)
	AC_DEFINE(USING_EXTERNAL_VIDEO_PLAYER, [], [Define that Tracker will try to use external video players])
	AM_CONDITIONAL(USING_LIBXINE, false)
	AM_CONDITIONAL(USING_GSTREAMER, false)
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
#  SQLite check
####################################################################

SQLITE_REQUIRED=3.4

PKG_CHECK_MODULES(SQLITE3, [ sqlite3 >= $SQLITE_REQUIRED ])

AC_SUBST(SQLITE3_CFLAGS)
AC_SUBST(SQLITE3_LIBS)



###############################################################################
# Check for file monitor backends
###############################################################################

FAM_LIBS=
inotify_support=no
inotify_linux=no
inotify_header=none
fam_support=no
fam_type=none

AM_CONDITIONAL(HAVE_INOTIFY, false)
AM_CONDITIONAL(HAVE_INOTIFY_LINUX, false)
AM_CONDITIONAL(HAVE_FAM, false)

AC_ARG_ENABLE(file-monitoring,
	AC_HELP_STRING(
		[--enable-file-monitoring=ARG],
		[enables one of the (inotify, fam, polling, auto) file monitoring backends]),,
	enable_file_monitoring=auto)


AC_CHECK_HEADERS([sys/inotify.h],
		 [
		 inotify_support=yes
		 inotify_linux=no
		 ])

AC_CHECK_HEADERS([linux/inotify.h],
		 [
		 inotify_linux=yes
		 inotify_support=yes
		 ])

AC_CHECK_LIB(fam, FAMOpen,
		[AC_CHECK_HEADERS(fam.h,
		[
		fam_support=yes
		fam_type=fam
		],[fam_support=no])],
		[fam_support=no])

PKG_CHECK_MODULES(GAMIN, 
		gamin,
		[
		fam_support=yes
		fam_type=gamin
		], 
		[
		fam_support=no
		])

if test "x$enable_file_monitoring" = "xauto"; then
	if test "$inotify_support"  = "yes"; then
		primary_backend="inotify"
		if test "$inotify_linux" = "yes"; then
			inotify_header="linux/inotify.h"
		else
			inotify_header="sys/inotify.h"
		fi

	elif test "$fam_support" = "yes"; then
		if test "$fam_type" = "gamin"; then
			primary_backend="gamin"
		else
			primary_backend="fam"
		fi
	else
		if test "$native_win32" = "yes"; then
			primary_backend="win32"
		else
			primary_backend="polling only"
		fi
	fi
elif test "x$enable_file_monitoring" = "xinotify"; then
	if test "$inotify_support" = "no"; then
		AC_MSG_ERROR([***inotify requested but not found - exiting!])
	else
		primary_backend="inotify"
		if test "$inotify_linux" = "yes"; then
			inotify_header="linux/inotify.h"
		else
			inotify_header="sys/inotify.h"
		fi
	fi
elif test "x$enable_file_monitoring" = "xfam"; then
	if test "$fam_support" = "no"; then
		AC_MSG_ERROR([***fam requested but not found - exiting!])
	else
		if test "$fam_type" = "gamin"; then
			primary_backend="gamin"
		else
			primary_backend="fam"
		fi
	fi
else
	if test "$native_win32" = "yes"; then
		primary_backend="win32"
	else
		primary_backend="polling only"
	fi
fi

if test "$primary_backend" = "inotify"; then
	AC_DEFINE(HAVE_INOTIFY, [], [Define if we have inotify])
	AM_CONDITIONAL(HAVE_INOTIFY, true)
	if test "$inotify_linux" = "yes"; then
			AC_DEFINE(HAVE_INOTIFY_LINUX, [], 
			[
			Define if we have inotify and header is linux/inotify.h
			])
			AM_CONDITIONAL(HAVE_INOTIFY_LINUX, true)
	fi
elif test "$primary_backend" = "gamin"; then 
	AC_DEFINE(HAVE_FAM, [], [Define if we have FAM])
	AM_CONDITIONAL(HAVE_FAM, true)
	FAM_LIBS="-lgamin-1"
	AC_SUBST(FAM_LIBS)
elif test "$primary_backend" = "fam"; then
	AC_DEFINE(HAVE_FAM, [], [Define if we have FAM])
	AM_CONDITIONAL(HAVE_FAM, true)
	FAM_LIBS="-lfam"
	AC_SUBST(FAM_LIBS)
fi


##################################################################
# Enable building libtracker-gtk?
##################################################################
LIBTRACKERGTK_GTK_REQUIRED=2.8.20


AC_ARG_ENABLE([libtrackergtk], AC_HELP_STRING([--disable-libtrackergtk], [Disable libtrackergtk]),,[enable_libtrackergtk=yes])
if test "x$enable_libtrackergtk" != "xno" ; then

   	libtrackergtk_modules="\
        	glib-2.0    >=  $GLIB_REQUIRED, \
        	gtk+-2.0    >=  $LIBTRACKERGTK_GTK_REQUIRED, \
        	dbus-1      >=  $DBUS_REQUIRED, \
        	dbus-glib-1 >=  $DBUS_REQUIRED"

	PKG_CHECK_MODULES(LIBTRACKERGTK, [$libtrackergtk_modules],[enable_libtrackergtk=yes] , [enable_libtrackergtk=no])

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

AC_ARG_ENABLE([trackerapplet], AC_HELP_STRING([--disable-trackerapplet], [Disable trackerapplet]),,[enable_trackerapplet=yes])
if test "x$enable_trackerapplet" != "xno" ; then

   	trackerapplet_modules="\
        	glib-2.0    >=  $LIBNOTIFY_GLIB_REQUIRED, \
        	gtk+-2.0    >=  $LIBNOTIFY_GTK2_REQUIRED, \
        	dbus-1      >=  $DBUS_REQUIRED, \
        	dbus-glib-1 >=  $DBUS_REQUIRED, \
		libnotify   >=  $LIBNOTIFY_REQUIRED"

	PKG_CHECK_MODULES(TRACKERAPPLET, [$trackerapplet_modules],[enable_trackerapplet=yes] , [enable_trackerapplet=no])

    	AC_SUBST([TRACKERAPPLET_CFLAGS])
    	AC_SUBST([TRACKERAPPLET_LIBS])

	
else
	enable_trackerapplet="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_TRACKERAPPLET, test "$enable_trackerapplet" = "yes")


##################################################################
# Enable UNAC support?
##################################################################


AC_ARG_ENABLE([unac], AC_HELP_STRING([--disable-unac], [Disable unac accent stripper support]),,[enable_unac=yes])

if test "x$enable_unac" != "xno" ; then

  	PKG_CHECK_MODULES(UNAC, [unac >= 1.0.0],[enable_unac=yes] , [enable_unac=no])

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
# check for GNOME/GTK dependencies to build tracker search tool (Gnome client)
##################################################################

GTK_REQUIRED=2.8.0
LIBGNOME_DESKTOP_REQUIRED=2.9.91
LIBGNOME_REQUIRED=2.13.2
LIBGNOMEUI_REQUIRED=2.13.7
GNOMEVFS_REQUIRED=2.8.4

AC_ARG_ENABLE(gui, AC_HELP_STRING([--disable-gui], [Disable building of the tracker-search-tool]),,[enable_gui=yes])
if test "x$enable_gui" = "xyes"; then
	PKG_CHECK_MODULES(GNOME_UTILS, [
        	gtk+-2.0 >= $GTK_REQUIRED
	        libgnome-2.0 >= $LIBGNOME_REQUIRED
        	libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED],
	        [have_gnome=yes] , [have_gnome=no])
	#there is the slight possibility to have gnome-vfs and not libgnome for example
	#so we should check and not overwrite have_gnome
	if test "$have_gnome" = "yes"; then 
		PKG_CHECK_MODULES(GNOMEVFS, [
        		gnome-vfs-2.0 >= $GNOMEVFS_REQUIRED
		        gnome-vfs-module-2.0 >= $GNOMEVFS_REQUIRED],
		        [have_gnome=yes] , [have_gnome=no])
	fi
	if test "$have_gnome" = "yes"; then
		PKG_CHECK_MODULES(GNOMEDESKTOP, [
		        gnome-desktop-2.0 >= $LIBGNOME_DESKTOP_REQUIRED],
        		[have_gnome=yes] , [have_gnome=no])
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
              AC_HELP_STRING([--disable-preferences], [Disable the preferences dialog]),,
              [enable_preferences=yes])

if test "x$enable_preferences" != "xno" ; then

   	PKG_CHECK_MODULES(GLIB2, [ glib-2.0 >= $PREFERENCES_GLIB_REQUIRED ],[enable_preference=yes] , [enable_preference=no])
   	AC_SUBST([GLIB2_CFLAGS])
   	AC_SUBST([GLIB2_LIBS])

	if test "$enable_preferences" = "yes"; then 
		PKG_CHECK_MODULES(GTK2, [ gtk+-2.0 >= $PREFERENCES_GTK_REQUIRED ],[enable_preference=yes] , [enable_preference=no])
		AC_SUBST([GTK2_CFLAGS])
   		AC_SUBST([GTK2_LIBS])
	fi

	if test "$enable_preferences" = "yes"; then 

	   	PKG_CHECK_MODULES(LIBGLADE, [ libglade-2.0 >= $PREFERENCES_GLADE_REQUIRED ], [enable_preferences=yes], [enable_preferences=no])
		AC_SUBST([LIBGLADE_CFLAGS])
   		AC_SUBST([LIBGLADE_LIBS])
	fi
else
	enable_preferences="no (disabled)"
fi

AM_CONDITIONAL(ENABLE_PREFERENCES, test "$enable_preferences" = "yes")

##################################################################
# check for poppler's glib bingings
##################################################################

POPPLER_GLIB_REQUIRED=0.4.5
CAIRO_REQUIRED=1.0
GDK_REQUIRED=1.0

AC_ARG_ENABLE(pdf, AC_HELP_STRING([--disable-pdf], [Disable PDF data extractor]),,[enable_pdf=yes])
if test "x$enable_pdf" = "xyes"; then
	PKG_CHECK_MODULES(POPPLER_GLIB, [
		poppler-glib >= $POPPLER_GLIB_REQUIRED
		cairo >= $CAIRO_REQUIRED
		gdk-2.0 >= $GDK_REQUIRED],
		[have_poppler=yes] , [have_poppler=no])
	AC_SUBST(POPPLER_GLIB_CFLAGS)
	AC_SUBST(POPPLER_GLIB_LIBS)
else
	have_poppler="no (disabled)"
fi
AM_CONDITIONAL(HAVE_POPPLER_GLIB, test "$have_poppler" = "yes")
test "$have_poppler" = "yes" && AC_DEFINE(HAVE_POPPLER, [], [Define if we have poppler])

##################################################################
# check for libexif
##################################################################

LIBEXIF_REQUIRED=0.6

AC_ARG_ENABLE(exif, AC_HELP_STRING([--disable-exif],[Disable exif data extractor]),,[enable_exif=yes])
if test "x$enable_exif" = "xyes"; then
	PKG_CHECK_MODULES(LIBEXIF, [
		libexif >= $LIBEXIF_REQUIRED],
		[have_libexif=yes] , [have_libexif=no])
	AC_SUBST(LIBEXIF_CFLAGS)
	AC_SUBST(LIBEXIF_LIBS)
else
	have_libexif="no (disabled)"
fi
AM_CONDITIONAL(HAVE_LIBEXIF, test "$have_libexif" = "yes")
test "$have_libexif" = "yes" && AC_DEFINE(HAVE_LIBEXIF, [], [Define if we have libexif])

##################################################################
# check for libgsf
##################################################################

LIBGSF_REQUIRED=1.13

AC_ARG_ENABLE(gsf, AC_HELP_STRING([--disable-gsf], [Disable GSF data extractor]),,[enable_gsf=yes])
if test "x$enable_gsf" = "xyes"; then
	PKG_CHECK_MODULES(LIBGSF,[
		libgsf-1 >= $LIBGSF_REQUIRED],
		[have_libgsf=yes] , [have_libgsf=no])
	AC_SUBST(LIBGSF_CFLAGS)
	AC_SUBST(LIBGSF_LIBS)
else
	have_libgsf="no (disabled)"
fi
AM_CONDITIONAL(HAVE_LIBGSF, test "$have_libgsf" = "yes")
test "$have_libgsf" = "yes" && AC_DEFINE(HAVE_LIBGSF, [], [Define if we have libgsf])

####################################################################
# Check ioprio support
####################################################################

AC_MSG_CHECKING([[checking ioprio support]])
ioprio_support=no
AC_RUN_IFELSE([ AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <errno.h> 
#include <sys/syscall.h>
#include <unistd.h>
inline int ioprio_get (int which, int who)
{
	return syscall (__NR_ioprio_get, which, who);
}
   ]], [[
exit(ioprio_get(1,0));
   ]]) ], 
   [ AC_DEFINE(IOPRIO_SUPPORT,[],[Define ioprio support]) ioprio_support=yes ])
AC_MSG_RESULT([$ioprio_support])

#####################################################

##################################################################
# check for exempi
##################################################################

EXEMPI_REQUIRED=1.99.2

AC_ARG_ENABLE(xmp, AC_HELP_STRING([--disable-xmp], [Disable XMP extraction]),,[enable_xmp=yes])
if test "x$enable_xmp" = "xyes"; then
	PKG_CHECK_MODULES(EXEMPI,[
		exempi-2.0 >= $EXEMPI_REQUIRED],
		[have_exempi=yes] , [have_exempi=no])
	AC_SUBST(EXEMPI_CFLAGS)
	AC_SUBST(EXEMPI_LIBS)
else
	have_exempi="no (disabled)"
fi
AM_CONDITIONAL(HAVE_EXEMPI, test "$have_exempi" = "yes")
test "$have_exempi" = "yes" && AC_DEFINE(HAVE_EXEMPI, [], [Define if we have exempi])

AC_CONFIG_FILES([
	Makefile
	tracker.pc
	trackerd.desktop.in
	libtracker-gtk.pc
	docs/Makefile
	data/Makefile
	data/languages/Makefile
	data/icons/Makefile
	data/icons/16x16/Makefile
	data/icons/22x22/Makefile
	data/icons/24x24/Makefile
	data/icons/32x32/Makefile
	data/icons/48x48/Makefile
	data/icons/scalable/Makefile
	data/services/Makefile
	filters/Makefile
	filters/application/Makefile
	filters/text/Makefile
	thumbnailers/Makefile
	thumbnailers/application/Makefile
	thumbnailers/image/Makefile
	src/Makefile
	src/libstemmer/Makefile
	src/qdbm/Makefile
	src/text-filters/Makefile
	src/text-filters/ooo_converter/Makefile
	src/tracker-extract/Makefile
	src/tracker-thumbnailer/Makefile
	src/trackerd/Makefile
	src/libtracker/Makefile
	src/libtracker-gtk/Makefile
	src/tracker-applet/Makefile
	src/tracker-applet/tracker-applet.desktop.in
	src/tracker-search-tool/Makefile
	src/tracker-search-tool/tracker-search-tool.desktop.in
	src/tracker-preferences/Makefile
	src/tracker-preferences/tracker-preferences.desktop.in	
	src/xdgmime/Makefile
	po/Makefile.in
	python/Makefile
	python/deskbar-handler/Makefile
])

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT

echo "
Tracker-$VERSION:

	prefix:					${prefix}
	source code location:			${srcdir}
	compiler:				${CC}
	enable gcc warnings:			$enable_warnings
	build with debug symbols:		$enable_debug_code
	file monitor backend to be used:	$primary_backend
	inotify header location:		$inotify_header
	ioprio support:				$ioprio_support
	Win32:					$native_win32
	HAL (battery detection) support:	$have_hal
	unac accent stripper support:		$enable_unac
	deskbar-applet support:			$enable_deskbar_applet
	build search tool (GNOME):		$have_gnome
	build preferences ui:			$enable_preferences
	build libtracker-gtk:			$enable_libtrackergtk
	tracker applet notification icon:	$enable_trackerapplet

Metadata extractors:

	pdf:					$have_poppler
	png:					yes
	exif (jpeg):				$have_libexif
	ms and openoffice (gsf):		$have_libgsf
	video files:				$videos_are_handled ($videos_handler)
	xml/html formats:			$have_libxml2
	embedded/sidecar xmp:			$have_exempi

"

echo "
****WARNING**** : You must make sure a version of SQLite compiled with --enable-threadsafe is available otherwise expect pain!
"

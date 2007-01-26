# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# This file is part of Tracker.

AC_PREREQ(2.57)
AC_INIT([tracker],[0.5.4],[tracker-list@gnome.org])

AC_CONFIG_SRCDIR([src/trackerd/trackerd.c])
AM_INIT_AUTOMAKE([dist-bzip2])

AC_SUBST(PACKAGE_URL, [http://www.tracker-project.org])

CFLAGS="$CFLAGS"

# Checks for programs.
AC_PROG_CC
AC_HEADER_STDC

AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h sitdlib.h string.h sys/time.h unistd.h linux/unistd.h])

AC_CHECK_HEADER([zlib.h],
                [],
                [AC_MSG_ERROR([You must have zlib.h and zlib installed])])

AC_CHECK_HEADER([magic.h],
                [],
                [AC_MSG_ERROR([You must have magic.h and libmagic installed])])

# Checks for functions
AC_CHECK_FUNCS([getline])

dnl Library Checks
DBUS_REQUIRED=0.60
GLIB_REQUIRED=2.9.1
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
# Deskbar Applet Handler
####################################################################

AC_ARG_WITH([deskbar_applet_handler_dir],
            AC_HELP_STRING([--with-deskbar-applet-handler-dir], [Path to Deskbar handler directory]))

if test "x$with_deskbar_applet_handler_dir" = "x" ; then
   handler_dir="`pkg-config --variable handlersdir deskbar-applet`"
else
   handler_dir="$with_deskbar_applet_handler_dir"
fi



DESKBAR_HANDLER_DIR="$handler_dir"
AC_SUBST(DESKBAR_HANDLER_DIR)



####################################################################
# Compiler warning checks
####################################################################
AC_ARG_ENABLE(warnings,
              AC_HELP_STRING([--disable-warnings], [disable GCC warnings]),,
              [enable_warnings=yes])

if test "x$enable_warnings" = "xyes"; then
   CFLAGS="\
   -Wall \
   -Wchar-subscripts \
   -Wnested-externs \
   -Wpointer-arith \
   -Wsign-compare \
   $CFLAGS"
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

ALL_LINGUAS="ar de en_GB es et fi fr gl it nb sl sv"
AC_SUBST(ALL_LINGUAS)

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext translation domain])

AM_GLIB_GNU_GETTEXT
AC_PROG_INTLTOOL([0.22])

# The following lines replace AM_GLIB_DEFINE_LOCALEDIR(TRACKER_LOCALEDIR)
# due to the bug with autoconf 2.60

dnl Set TRACKER_LOCALEDIR in config.h.
if test "x${prefix}" = "xNONE";
then
	TRACKER_LOCALEDIR="${ac_default_prefix}/${DATADIRNAME}/locale"
else
	TRACKER_LOCALEDIR="${prefix}/${DATADIRNAME}/locale"
fi

AC_DEFINE_UNQUOTED(TRACKER_LOCALEDIR, "$TRACKER_LOCALEDIR",
		[The directory in which locale data will be stored])

# end of AM_GLIB_DEFINE_LOCALEDIR(TRACKER_LOCALEDIR) replacement

####################################################################
# check for newer glib (>= 2.12.0) for g_option_context_set_summary ()
# availability
####################################################################
RECENT_GLIB=2.12.0

PKG_CHECK_MODULES(glib2, [ glib-2.0 >= $RECENT_GLIB ],
                  [have_recent_glib=yes], [have_recent_glib=no])

if test "$have_recent_glib" = "yes" ; then
   AC_DEFINE(HAVE_RECENT_GLIB, 1, [Define if we have recent glib])
fi

AM_CONDITIONAL(HAVE_RECENT_GLIB, test "$have_recent_glib" = "yes")

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
# External SQLite check
####################################################################

SQLITE_REQUIRED=3.2

AC_ARG_ENABLE(external_sqlite, 
	AC_HELP_STRING(
		[--enable-external-sqlite],
		[build using system's sqlite which must be a threadsafe version]),,
	[enable_external_sqlite=no])

if test "x$enable_external_sqlite" = "xyes"; then
	PKG_CHECK_MODULES(SQLITE3, [ sqlite3 >= $SQLITE_REQUIRED ])
	AM_CONDITIONAL(USING_EXTERNAL_SQLITE, true)
else
	SQLITE3_CFLAGS="-I\$(top_srcdir)/src/sqlite3"
	SQLITE3_LIBS="\$(top_builddir)/src/sqlite3/libsqlite-private.la"
	AM_CONDITIONAL(USING_EXTERNAL_SQLITE, false)
fi		

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
		primary_backend=inotify
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
		primary_backend="polling only"
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
	primary_backend="polling only"
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
# check for GNOME/GTK dependencies to build tracker search tool (Gnome client)
##################################################################

GTK_REQUIRED=2.8.0
LIBGNOME_DESKTOP_REQUIRED=2.9.91
LIBGNOME_REQUIRED=2.13.2
LIBGNOMEUI_REQUIRED=2.13.7
LIBGLADE_REQUIRED=2.5.0
GNOMEVFS_REQUIRED=2.8.4

AC_ARG_ENABLE(gui, AC_HELP_STRING([--disable-gui], [Disable building of the gnome-search-tool]),,[enable_gui=yes])
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
AM_CONDITIONAL(HAVE_GNOME, test "$have_gnome" = "yes")
if test "$have_gnome" = "yes"; then
   GNOME_COMMON_INIT
fi


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

AC_CONFIG_FILES([
	Makefile
	tracker.pc
	docs/Makefile
	data/Makefile
	data/languages/Makefile
	filters/Makefile
	filters/application/Makefile
	filters/text/Makefile
	thumbnailers/Makefile
	thumbnailers/application/Makefile
	thumbnailers/image/Makefile
	src/Makefile
	src/libstemmer/Makefile
	src/sqlite3/Makefile
	src/text-filters/Makefile
	src/text-filters/html_converter/Makefile
	src/text-filters/ooo_converter/Makefile
	src/tracker-extract/Makefile
	src/tracker-thumbnailer/Makefile
	src/trackerd/Makefile
	src/libtracker/Makefile
	src/tracker-search-tool/Makefile
	src/xdgmime/Makefile
	po/Makefile.in
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
	database engine:			SQLite
	file monitor backend to be used:	$primary_backend
	inotify header location:		$inotify_header
	ioprio support:				$ioprio_support
	build search tool (GNOME):		$have_gnome

Metadata extractors:

	pdf:					$have_poppler
	png:					yes
	exif (jpeg):				$have_libexif
	gsf:					$have_libgsf
	video files:				$videos_are_handled ($videos_handler)

"
if test "x$enable_external_sqlite" = "xyes"; then
echo "
****WARNING**** : you have chosen to use an external version of SQLite instead of the in-built threadsafe one. 
You must make sure a version of SQLite compiled with --enable-threadsafe is available otherwise expect pain!
"
fi

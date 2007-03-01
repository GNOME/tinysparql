#@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

icondir = $(datadir)/pixmaps
desktopdir = $(datadir)/applications

desktop_in_files = tracker-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)


icon_DATA = tracker.png
pkgdata_DATA = tracker-preferences.glade

noinst_HEADERS = tracker-configuration.h \
		 tracker-configuration-private.h \
		 tracker-preferences-dialogs.h \
		 tracker-preferences.h \
		 tracker-preferences-private.h \
		 tracker-preferences-utils.h \
		 $(NULL)

INCLUDES = -DPIXMAPS_DIR=\""$(datadir)/pixmaps"\" \
	   -DTRACKER_DATADIR=\""$(datadir)/tracker"\" \
	   $(GLIB_CFLAGS) \
	   $(GTK2_CFLAGS) \
	   $(LIBGLADE_CFLAGS) \
	   $(NULL)

tracker_preferences_SOURCES = tracker-configuration.c \
			      tracker-preferences.c \
			      tracker-preferences-dialogs.c \
			      tracker-preferences-main.c \
			      tracker-preferences-utils.c \
			      $(NULL)

tracker_preferences_LDADD = $(GLIB_LIBS) \
			    $(GTK2_LIBS) \
			    $(LIBGLADE_LIBS) \
			    $(NULL)

EXTRA_DIST = $(desktop_in_files) $(icon_DATA) $(pkgdata_DATA)

DISTCLEANFILES = $(desktop_DATA)
MAINTAINERCLEANFILES = Makefile.in

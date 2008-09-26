include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-I$(top_srcdir)/src						\
	$(TRACKERAPPLET_CFLAGS)						\
	$(LIBGLADE_CFLAGS)

bin_PROGRAMS = tracker-applet

tracker_applet_LDADD = 							\
	$(top_builddir)/src/libtracker/libtrackerclient.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(TRACKERAPPLET_LIBS)						\
	$(LIBGLADE_LIBS)

tracker_applet_SOURCES = 						\
	tracker-applet.c 						\
	tracker-applet.h 						\
	tracker-applet-marshallers.c 					\
	tracker-applet-marshallers.h

icondir = $(datadir)/tracker/icons

icon_DATA = 								\
	tracker-applet-default.png 					\
	tracker-applet-indexing1.png 					\
	tracker-applet-indexing2.png 					\
	tracker-applet-paused.png

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = tracker-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

pkgdata_DATA = tracker-applet-prefs.glade


EXTRA_DIST = $(icon_DATA) $(pkgdata_DATA)

CLEANFILES = $(autostart_DATA)

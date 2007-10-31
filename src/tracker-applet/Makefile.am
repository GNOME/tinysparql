
@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = tracker-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

icondir = $(datadir)/tracker/icons

INCLUDES = \
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" \
	-DTRACKER_DATADIR=\""$(datadir)"\" \
	-I$(top_srcdir)/src/libtracker \
	$(TRACKERAPPLET_CFLAGS)

bin_PROGRAMS = tracker-applet

noinst_HEADERS = \
	tracker-applet.h \
	tracker-applet-private.h \
	tracker-applet-marshallers.h

tracker_applet_LDADD = \
	$(top_builddir)/src/libtracker/libtrackerclient.la \
	$(TRACKERAPPLET_LIBS)

tracker_applet_SOURCES = \
	tracker-applet.c \
	tracker-applet-marshallers.c

icon_DATA = \
	tracker-applet-default.png \
	tracker-applet-indexing1.png \
	tracker-applet-indexing2.png \
	tracker-applet-paused.png

EXTRA_DIST = $(icon_DATA)

CLEANFILES = $(autostart_DATA)

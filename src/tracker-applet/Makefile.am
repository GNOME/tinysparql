
@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = tracker-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

INCLUDES = -DTRACKER_LOCALEDIR=\""$(localedir)"\" \
	   -I$(top_srcdir)/src/libtracker \
	   $(TRACKERAPPLET_CFLAGS)

bin_PROGRAMS = tracker-applet

noinst_HEADERS = tracker-applet.h \
		 tracker-applet-private.h \
		 tracker-applet-marshallers.h  \
		 $(NULL)

tracker_applet_LDADD = $(top_srcdir)/src/libtracker/libtrackerclient.la $(TRACKERAPPLET_LIBS)

tracker_applet_SOURCES = tracker-applet.c \
			 tracker-applet-marshallers.c  \
			 $(NULL)

CLEANFILES = $(autostart_DATA)

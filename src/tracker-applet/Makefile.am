
@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = tracker-applet.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

INCLUDES = -DTRACKER_LOCALEDIR=\""$(localedir)"\" \
	   -I$(top_srcdir)/src/libtracker \
	   $(TRACKERAPPLET_CFLAGS)

bin_PROGRAMS = tracker-applet

noinst_HEADERS = tm-utils.h \
		 tm-debug.h \
		 tm-common.h \
		 tm-tray-icon.h \
		 tm-tray-icon-private.h \
		 tm-trackerd-connection.h \
		 tm-trackerd-connection-private.h \
		 $(NULL)

tracker_applet_LDADD = $(TRACKERAPPLET_LIBS)

tracker_applet_SOURCES = tm-main.c \
			 tm-debug.c \
			 tm-utils.c \
			 tm-tray-icon.c \
			 tm-trackerd-connection.c \
			 $(NULL)

CLEANFILES = $(autostart_DATA)

bin_PROGRAMS = tracker-monitor

noinst_HEADERS = tm-utils.h \
		 tm-debug.h \
		 tm-common.h \
		 tm-tray-icon.h \
		 tm-tray-icon-private.h \
		 tm-trackerd-connection.h \
		 tm-trackerd-connection-private.h \
		 $(NULL)

INCLUDES = -DVERSION="\"$(VERSION)\"" \
	   -DDATADIR="\"$(DATADIR)\"" \
	   -I$(top_srcdir)/src/libtracker \
	   $(TRACKERMONITOR_CFLAGS)

tracker_monitor_LDADD = $(TRACKERMONITOR_LIBS)

tracker_monitor_SOURCES = tm-main.c \
			  tm-debug.c \
			  tm-utils.c \
			  tm-tray-icon.c \
			  tm-trackerd-connection.c \
			  $(NULL)

MAINTAINERCLEANFILES = Makefile.in

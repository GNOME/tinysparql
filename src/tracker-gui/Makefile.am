INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GTK_CFLAGS)			\
	$(GLIB2_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(CFLAGS) 			\
	-g


bin_PROGRAMS = tracker-gui

tracker_gui_SOURCES = 	tracker-gui.c 		\
			sexy-icon-entry.c	\
			sexy-icon-entry.h



tracker_gui_LDADD = 	$(GTK_LIBS)	\
			$(GLIB2_LIBS) 	\
		 	$(DBUS_LIBS)	\
			$(top_builddir)/src/libtracker/libmetatracker.la 

tracker_gui_LDFLAGS = -Wl,--export-dynamic

configdir = $(datadir)/tracker

config_DATA =   tracker.glade


GNOME_CFLAGS=$(shell pkg-config --cflags libgnomeui-2.0)
GNOME_LIBS=$(shell pkg-config --libs libgnomeui-2.0)

INCLUDES =				\
	-DDATADIR=\""$(datadir)"\"	\
	$(GTK_CFLAGS)			\
	$(GLIB2_CFLAGS)			\
	$(DBUS_CFLAGS)			\
   $(GNOME_CFLAGS)   \
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

tracker_gui_LDFLAGS = -Wl,--export-dynamic   \
		 	$(GNOME_LIBS)

configdir = $(datadir)/tracker

config_DATA =   tracker.glade


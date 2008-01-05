@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_in_files = tracker-preferences.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)


pkgdata_DATA = tracker-preferences.glade

noinst_HEADERS = tracker-configuration.h \
		 tracker-preferences-dialogs.h \
		 tracker-preferences.h \
		 tracker-preferences-utils.h \
		 $(NULL)

INCLUDES =	-DTRACKER_DATADIR=\""$(datadir)/tracker"\" \
		-DTRACKER_LOCALEDIR=\""$(localedir)"\" \
		-DTRACKER_BINDIR=\""$(bindir)"\" \
		$(GLIB_CFLAGS) \
		$(GTK2_CFLAGS) \
		$(LIBGLADE_CFLAGS) \
		$(DBUS_CFLAGS) \
		-I$(top_srcdir)/src/libtracker \
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
				$(DBUS_LIBS) \
				$(top_builddir)/src/libtracker/libtrackerclient.la \
			    $(NULL)

EXTRA_DIST = $(pkgdata_DATA)

CLEANFILES = $(desktop_DATA)

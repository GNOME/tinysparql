include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_DATA =  tracker-preferences.desktop

pkgdata_DATA = tracker-preferences.glade

INCLUDES = 								\
	-DTRACKER_DATADIR=\""$(datadir)/tracker"\" 			\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" 				\
	-DTRACKER_BINDIR=\""$(bindir)"\" 				\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DTRACKER_COMPILATION 						\
	$(WARN_CFLAGS)							\
	$(LIBGLADE_CFLAGS) 						\
	$(GTK2_CFLAGS) 							\
	$(DBUS_CFLAGS) 							\
	$(GLIB_CFLAGS) 							\
	-I$(top_srcdir)/src

tracker_preferences_SOURCES = 						\
	tracker-preferences.c 						\
	tracker-preferences.h 						\
	tracker-preferences-dialogs.c 					\
	tracker-preferences-dialogs.h 					\
	tracker-preferences-main.c 					\
	tracker-preferences-utils.c 					\
	tracker-preferences-utils.h

tracker_preferences_LDADD = 						\
	$(top_builddir)/src/libtracker/libtrackerclient.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(LIBGLADE_LIBS)						\
	$(GTK2_LIBS) 							\
	$(DBUS_LIBS) 							\
	$(GLIB_LIBS)

EXTRA_DIST = $(pkgdata_DATA)

CLEANFILES = $(desktop_DATA)

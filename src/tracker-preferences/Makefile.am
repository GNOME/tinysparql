include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_DATA =  tracker-preferences.desktop

pkgdata_DATA = tracker-preferences.ui

INCLUDES = 								\
	-DTRACKER_DATADIR=\""$(datadir)/tracker"\" 			\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" 				\
	-DTRACKER_BINDIR=\""$(bindir)"\" 				\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DTRACKER_COMPILATION 						\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"			\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB_CFLAGS) 							\
	$(GCOV_CFLAGS) 							\
	$(GTK2_CFLAGS) 							\
	$(DBUS_CFLAGS)							\
	$(GMODULE_CFLAGS)

BUILT_SOURCES = tracker-preferences.vala.stamp

config_class_sources = \
	$(top_srcdir)/src/tracker-miner-fs/tracker-config.h		\
	$(top_srcdir)/src/tracker-miner-fs/tracker-config.c

tracker_preferences_VALASOURCES = tracker-preferences.vala

tracker_preferences_SOURCES = 						\
	$(config_class_sources)						\
	$(tracker_preferences_VALASOURCES:.vala=.c)

tracker_preferences_LDADD = 						\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GTK2_LIBS) 							\
	$(DBUS_LIBS) 							\
	$(GCOV_LIBS) 							\
	$(GLIB_LIBS)							\
	$(GMODULE_LIBS)

EXTRA_DIST = $(pkgdata_DATA) tracker-config.vapi \
	$(tracker_preferences_VALASOURCES)

tracker-preferences.vala.stamp: $(tracker_preferences_VALASOURCES)
	$(VALAC) --pkg gtk+-2.0 --pkg gmodule-2.0 -C $(VALAFLAGS) -H tracker-preferences.h tracker-config.vapi config.vapi $^
	touch $@

CLEANFILES = $(desktop_DATA) tracker-preferences.h tracker-preferences.c tracker-preferences.vala.stamp

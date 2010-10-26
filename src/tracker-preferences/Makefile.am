include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_DATA =  tracker-preferences.desktop

pkgdata_DATA = tracker-preferences.ui

AM_CPPFLAGS = 								\
	$(BUILD_CFLAGS)							\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-DTRACKER_DATADIR=\""$(datadir)/tracker"\" 			\
	-DTRACKER_LOCALEDIR=\""$(localedir)"\" 				\
	-DTRACKER_BINDIR=\""$(bindir)"\" 				\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"			\
	$(TRACKER_PREFERENCES_CFLAGS)

BUILT_SOURCES = 							\
	tracker-preferences.vala.stamp

config_class_sources = \
	$(top_srcdir)/src/miners/fs/tracker-config.h			\
	$(top_srcdir)/src/miners/fs/tracker-config.c			\
	$(top_srcdir)/src/tracker-status-icon/tracker-icon-config.h	\
	$(top_srcdir)/src/tracker-status-icon/tracker-icon-config.c

tracker_preferences_VALASOURCES = tracker-preferences.vala

tracker_preferences_SOURCES = 						\
	$(config_class_sources)						\
	$(tracker_preferences_VALASOURCES:.vala=.c)

tracker_preferences_LDADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(BUILD_LIBS)							\
	$(TRACKER_PREFERENCES_LIBS)

MAINTAINERCLEANFILES =							\
	tracker-preferences.vala.stamp					\
	$(tracker_preferences_VALASOURCES:.vala=.c)

EXTRA_DIST = 								\
	$(pkgdata_DATA) 						\
	config.vapi							\
	tracker-config.vapi						\
	tracker-status-icon-config.vapi					\
	tracker-preferences.vala.stamp 					\
	$(tracker_preferences_VALASOURCES)

tracker-preferences.vala.stamp: $(tracker_preferences_VALASOURCES) tracker-config.vapi tracker-status-icon-config.vapi config.vapi
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) --pkg gtk+-2.0 --pkg gmodule-2.0 -C $(VALAFLAGS) $^
	$(AM_V_GEN)touch $@

CLEANFILES = $(desktop_DATA)

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
	-I$(top_builddir)						\
	$(TRACKER_APPS_CFLAGS)						\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)

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
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(TRACKER_APPS_LIBS)						\
	$(GCOV_LIBS)

MAINTAINERCLEANFILES =							\
	tracker-preferences.vala.stamp					\
	$(tracker_preferences_VALASOURCES:.vala=.c)			\
	$(NULL)

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

include $(top_srcdir)/Makefile.decl

@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_DATA =  tracker-preferences.desktop

pkgdata_DATA = tracker-preferences.ui

AM_VALAFLAGS =                                         \
	--pkg gtk+-2.0 --pkg gmodule-2.0               \
	$(BUILD_VALAFLAGS)                             \
	config.vapi                                    \
	tracker-config.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DTRACKER_DATADIR=\""$(datadir)/tracker"\"     \
	-DTRACKER_LOCALEDIR=\""$(localedir)"\"         \
	-DTRACKER_BINDIR=\""$(bindir)"\"               \
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\"       \
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"       \
	$(TRACKER_PREFERENCES_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_PREFERENCES_LIBS)

tracker_preferences_SOURCES =                          \
	tracker-preferences.vala                       \
	$(top_srcdir)/src/miners/fs/tracker-config.c

EXTRA_DIST =                                           \
	$(pkgdata_DATA)                                \
	config.vapi                                    \
	tracker-config.vapi

CLEANFILES = $(desktop_DATA)

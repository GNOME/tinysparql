@INTLTOOL_DESKTOP_RULE@

bin_PROGRAMS = tracker-preferences

desktopdir = $(datadir)/applications
desktop_DATA =  tracker-preferences.desktop

appdatadir = $(datadir)/appdata
appdata_DATA =  tracker-preferences.appdata.xml

pkgdata_DATA = tracker-preferences.ui

AM_VALAFLAGS =                                         \
	--pkg gtk+-3.0 --pkg gmodule-2.0               \
	$(BUILD_VALAFLAGS)                             \
	config.vapi                                    \
	tracker-config.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_VALACFLAGS)                            \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"       \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DSRCDIR=\"$(abs_srcdir)/\"                    \
	$(TRACKER_PREFERENCES_CFLAGS)                  \
	-include config.h

LDADD =                                                \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_PREFERENCES_LIBS)

tracker_preferences_SOURCES =                          \
	tracker-preferences.vala                       \
	$(top_srcdir)/src/miners/fs/tracker-config.c

EXTRA_DIST =                                           \
	$(pkgdata_DATA)                                \
	$(appdata_DATA)                                \
	config.vapi                                    \
	tracker-config.vapi

CLEANFILES = $(desktop_DATA)

include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-needle

AM_VALAFLAGS =                                         \
	--pkg dbus-glib-1 --pkg gtk+-2.0               \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	config.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"       \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DSRCDIR=\"$(abs_srcdir)/\"                    \
	$(TRACKER_NEEDLE_CFLAGS)                       \
	-include config.h

LDADD =                                                \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_NEEDLE_LIBS)

tracker_needle_SOURCES =                               \
	tracker-cell-renderer-text.vala                \
	tracker-config.vala                            \
	tracker-query.vala                             \
	tracker-stats.vala                             \
	tracker-taglist.vala                           \
	tracker-utils.vala                             \
	tracker-needle.vala                            \
	tracker-view.vala

uidir = $(datadir)/tracker
ui_DATA = tracker-needle.ui

BUILT_SOURCES =                                        \
	tracker_needle_vala.stamp

EXTRA_DIST =                                           \
	$(ui_DATA)                                     \
	config.vapi


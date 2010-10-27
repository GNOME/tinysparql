include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-needle

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

tracker_needle_VALASOURCES =                           \
	tracker-cell-renderer-text.vala                \
	tracker-config.vala                            \
	tracker-needle.vala                            \
	tracker-query.vala                             \
	tracker-stats.vala                             \
	tracker-taglist.vala                           \
	tracker-utils.vala                             \
	tracker-view.vala

tracker_needle_SOURCES =                               \
	$(tracker_needle_VALASOURCES:.vala=.c)

vapi_sources =                                         \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	config.vapi

tracker-needle.vala.stamp: $(tracker_needle_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) -g --pkg dbus-glib-1 --pkg gtk+-2.0 $^
	$(AM_V_GEN)touch $@

uidir = $(datadir)/tracker
ui_DATA = tracker-needle.ui

BUILT_SOURCES =                                        \
	tracker-needle.vala.stamp

MAINTAINERCLEANFILES =                                 \
	$(BUILT_SOURCES)                               \
	$(tracker_needle_VALASOURCES:.vala=.c)

EXTRA_DIST =                                           \
	$(needle_VALASOURCES)                          \
	$(ui_DATA)                                     \
	config.vapi                                    \
	tracker-needle.vala.stamp

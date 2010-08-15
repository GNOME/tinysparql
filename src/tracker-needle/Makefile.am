include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-needle

tracker_needle_VALASOURCES = 					\
	tracker-cell-renderer-text.vala				\
	tracker-needle.vala \
	tracker-stats.vala \
	tracker-utils.vala

tracker_needle_SOURCES = 					\
	$(tracker_needle_VALASOURCES:.vala=.c)

vapi_sources = \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	config.vapi

tracker-needle.vala.stamp: $(tracker_needle_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) -g --pkg dbus-glib-1 --pkg gtk+-2.0 $^
	$(AM_V_GEN)touch $@

tracker_needle_CFLAGS = 					\
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DLOCALEDIR=\""$(localedir)"\"				\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_VALA_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	-include config.h

tracker_needle_LDADD = 						\
	$(TRACKER_APPS_LIBS)					\
	$(TRACKER_VALA_LIBS)					\
	$(GCOV_LIBS)

uidir = $(datadir)/tracker
ui_DATA = tracker-needle.ui

EXTRA_DIST = 							\
	$(needle_VALASOURCES) 					\
	$(ui_DATA)						\
	config.vapi						\
	tracker-needle.vala.stamp

MAINTAINERCLEANFILES = 						\
	tracker-needle.vala.stamp 				\
	$(tracker_needle_VALASOURCES:.vala=.c)

BUILT_SOURCES = 						\
	tracker-needle.vala.stamp


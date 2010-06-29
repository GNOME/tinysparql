include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker_needle

tracker_needle_VALASOURCES = 					\
	tracker-needle.vala

tracker_needle_SOURCES = 					\
	$(tracker_needle_VALASOURCES:.vala=.c)

needle.vala.stamp: $(tracker_needle_VALASOURCES) config.vapi
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gtk+-2.0 $^
	touch tracker-needle.vala.stamp

tracker_needle_CFLAGS = 					\
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DLOCALEDIR=\""$(localedir)"\"				\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_VALA_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)

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


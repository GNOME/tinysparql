include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = needle

needle_VALASOURCES = 						\
	needle.vala

needle_SOURCES = 							\
	$(needle_VALASOURCES:.vala=.c)

needle.vala.stamp: $(needle_VALASOURCES) config.vapi
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gtk+-2.0 $^
	touch needle.vala.stamp

needle_CFLAGS = 							\
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"\
	-DLOCALEDIR=\""$(localedir)"\"			\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_VALA_CFLAGS)					\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)

needle_LDADD = 								\
	$(TRACKER_APPS_LIBS)					\
	$(TRACKER_VALA_LIBS)					\
	$(GCOV_LIBS)

uidir = $(datadir)/tracker
ui_DATA = needle.ui

EXTRA_DIST = 								\
	$(needle_VALASOURCES) 					\
	$(ui_DATA)								\
	config.vapi								\
	needle.vala.stamp

MAINTAINERCLEANFILES = 						\
	needle.vala.stamp 						\
	$(needle_VALASOURCES:.vala=.c)

BUILT_SOURCES = 							\
	needle.vala.stamp


include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-explorer

tracker_explorer_VALASOURCES = 					\
	tracker-explorer.vala

tracker_explorer_SOURCES = 					\
	$(tracker_explorer_VALASOURCES:.vala=.c)

tracker_explorer.vala.stamp: $(tracker_explorer_VALASOURCES)
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gee-1.0 --pkg gtk+-2.0 $^
	touch tracker_explorer.vala.stamp

tracker_explorer_CFLAGS = 					\
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(TRACKER_APPS_CFLAGS)					\
	$(TRACKER_VALA_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)

tracker_explorer_LDADD = 					\
	$(TRACKER_APPS_LIBS)					\
	$(TRACKER_VALA_LIBS)					\
	$(GCOV_LIBS)

uidir = $(datadir)/tracker
ui_DATA = tracker-explorer.ui

EXTRA_DIST = 							\
	$(tracker_explorer_VALASOURCES) 			\
	$(ui_DATA)						\
	tracker_explorer.vala.stamp

MAINTAINERCLEANFILES = 						\
	tracker_explorer.vala.stamp 				\
	$(tracker_explorer_VALASOURCES:.vala=.c)

BUILT_SOURCES = 						\
	tracker_explorer.vala.stamp


include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = tracker-explorer

tracker_explorer_VALASOURCES = \
	explorer.vala \
	$(NULL)


tracker_explorer_SOURCES = \
	$(tracker_explorer_VALASOURCES:.vala=.c) \
	$(NULL)

tracker_explorer.vala.stamp: $(tracker_explorer_VALASOURCES)
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gee-1.0 --pkg gtk+-2.0 $^
	touch tracker_explorer.vala.stamp

tracker_explorer_CFLAGS = \
	-DTRACKER_UI_DIR=\"$(datadir)/tracker/\"		\
	-DSRCDIR=\"$(abs_srcdir)/\"				\
	$(WARN_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(TRACKER_EXPLORER_CFLAGS)				\
	$(NULL)

tracker_explorer_LDADD = \
	$(GCOV_LIBS)						\
	$(TRACKER_EXPLORER_LIBS)				\
	$(NULL)

uidir = $(datadir)/tracker
ui_DATA = \
	explorer.ui


EXTRA_DIST = \
	$(tracker_explorer_SOURCES) \
	$(tracker_explorer_VALASOURCES) \
	tracker_explorer.vala.stamp \
	explorer.ui \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(tracker_explorer_SOURCES) \
	tracker_explorer.vala.stamp \
	$(NULL)

BUILT_SOURCES = \
	tracker_explorer.vala.stamp \
	$(NULL)


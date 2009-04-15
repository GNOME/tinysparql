include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(UUID_CFLAGS)							\
	$(RAPTOR_CFLAGS)						\
	$(GCOV_CFLAGS)

libtracker_datadir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_data_LTLIBRARIES = libtracker-data.la

libtracker_data_la_SOURCES = 						\
	tracker-data-backup.c						\
	tracker-data-manager.c						\
	tracker-data-query.c						\
	tracker-data-search.c						\
	tracker-data-update.c						\
	tracker-query-tree.c						\
	tracker-turtle.c

noinst_HEADERS =							\
	tracker-data-backup.h						\
	tracker-data-manager.h						\
	tracker-data-query.h						\
	tracker-data-search.h						\
	tracker-data-update.h						\
	tracker-query-tree.h						\
	tracker-turtle.h

libtracker_data_la_LDFLAGS =						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_data_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la		\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(UUID_LIBS)							\
	$(RAPTOR_LIBS)							\
	$(GCOV_LIBS)							\
	-lz


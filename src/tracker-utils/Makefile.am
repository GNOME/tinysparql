include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(DBUS_CFLAGS)

libs = 									\
	$(top_builddir)/src/libtracker-miner/libtracker-miner.la	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

bin_PROGRAMS = 								\
	tracker-import							\
	tracker-info							\
	tracker-processes						\
	tracker-search 							\
	tracker-sparql							\
	tracker-stats							\
	tracker-status							\
	tracker-tag

tracker_search_SOURCES = tracker-search.c
tracker_search_LDADD = $(libs)

tracker_stats_SOURCES = tracker-stats.c
tracker_stats_LDADD = $(libs)

tracker_tag_SOURCES = tracker-tag.c
tracker_tag_LDADD = $(libs)

tracker_status_SOURCES = tracker-status.c
tracker_status_LDADD = $(libs)

tracker_info_SOURCES = tracker-info.c
tracker_info_LDADD = $(libs)

tracker_processes_SOURCES = tracker-processes.c
tracker_processes_LDADD = $(libs)

tracker_sparql_SOURCES = tracker-sparql.c
tracker_sparql_LDADD = $(libs)

tracker_import_SOURCES = tracker-import.c
tracker_import_LDADD = $(libs)


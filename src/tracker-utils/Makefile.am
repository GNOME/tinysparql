include $(top_srcdir)/Makefile.decl

INCLUDES =							\
	-DLOCALEDIR=\""$(localedir)"\" 				\
	-DG_LOG_DOMAIN=\"Tracker\"				\
	-I$(top_srcdir)/src					\
	$(DBUS_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(GLIB2_CFLAGS)

libs = 								\
	$(top_builddir)/src/libtracker/libtrackerclient.la	\
	$(GIO_LIBS)						\
	$(GLIB2_LIBS)

bin_PROGRAMS = 							\
	tracker-search 						\
	tracker-query 						\
	tracker-meta-folder					\
	tracker-stats						\
	tracker-tag						\
	tracker-files						\
	tracker-status						\
	tracker-unique

tracker_search_SOURCES = tracker-search.c
tracker_search_LDADD = $(libs)

tracker_query_SOURCES = tracker-query.c
tracker_query_LDADD = $(libs)

tracker_meta_folder_SOURCES = tracker-meta-folder.c
tracker_meta_folder_LDADD = $(libs)

tracker_stats_SOURCES = tracker-stats.c
tracker_stats_LDADD = $(libs)

tracker_tag_SOURCES = tracker-tag.c
tracker_tag_LDADD = $(libs)

tracker_files_SOURCES = tracker-files.c
tracker_files_LDADD = $(libs)

tracker_status_SOURCES = tracker-status.c
tracker_status_LDADD = $(libs)

tracker_unique_SOURCES = tracker-unique.c
tracker_unique_LDADD = $(libs)

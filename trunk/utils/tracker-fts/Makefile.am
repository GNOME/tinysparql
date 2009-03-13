include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = tracker-fts-test

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(SQLITE3_CFLAGS)

tracker_fts_test_SOURCES =						\
	tracker-fts-test.c

tracker_fts_test_LDADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(SQLITE3_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GLIB2_LIBS)							\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		

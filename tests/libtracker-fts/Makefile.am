include $(top_srcdir)/Makefile.decl

SUBDIRS = 			\
	limits			\
	prefix

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-fts

INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-DTOP_BUILDDIR=\"$(top_builddir)\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

common_ldadd =								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la	 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

tracker_fts_SOURCES = 						\
	tracker-fts-test.c

tracker_fts_LDADD = $(common_ldadd)

EXTRA_DIST = 				\
	data.ontology			\
	fts3aa-data.rq			\
	fts3aa-1.rq			\
	fts3aa-1.out			\
	fts3aa-2.rq			\
	fts3aa-2.out			\
	fts3ae-data.rq			\
	fts3ae-1.rq			\
	fts3ae-1.out


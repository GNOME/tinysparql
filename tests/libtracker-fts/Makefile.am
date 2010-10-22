include $(top_srcdir)/Makefile.decl

SUBDIRS = 			\
	limits			\
	prefix

noinst_PROGRAMS = 							\
	$(TEST_PROGS) 							\
	tracker-parser


TEST_PROGS += 								\
	tracker-fts-test 						\
	tracker-parser-test

INCLUDES = 								\
	$(BUILD_CFLAGS)							\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-DTOP_BUILDDIR=\"$(top_builddir)\"				\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	-I$(top_builddir)/src						\
	-I$(top_srcdir)/src/libtracker-client				\
	-I$(top_builddir)/src/libtracker-client				\
	-I$(top_srcdir)/src/libtracker-data				\
	-I$(top_builddir)/src/libtracker-data				\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_builddir)/src/libtracker-sparql				\
	$(DBUS_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

common_ldadd =								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la	 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(BUILD_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

tracker_fts_test_SOURCES = tracker-fts-test.c

tracker_fts_test_LDADD = $(common_ldadd)

tracker_parser_test_SOURCES = tracker-parser-test.c

tracker_parser_test_LDADD = $(common_ldadd)

tracker_parser_SOURCES = tracker-parser.c

tracker_parser_LDADD = $(common_ldadd)

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


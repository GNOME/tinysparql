include $(top_srcdir)/Makefile.decl

SUBDIRS =                                              \
	limits                                         \
	prefix

noinst_PROGRAMS =                                      \
	$(TEST_PROGS)                                  \
	tracker-parser

TEST_PROGS +=                                          \
	tracker-fts-test                               \
	tracker-parser-test

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-I$(top_srcdir)/tests/common                   \
	-DTOP_SRCDIR=\"$(top_srcdir)\"                 \
	-DTOP_BUILDDIR=\"$(top_builddir)\"             \
	$(LIBTRACKER_FTS_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/tests/common/libtracker-testcommon.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_FTS_LIBS)

tracker_fts_test_SOURCES = tracker-fts-test.c

tracker_parser_test_SOURCES = tracker-parser-test.c

tracker_parser_SOURCES = tracker-parser.c

EXTRA_DIST =                                           \
	data.ontology                                  \
	fts3aa-data.rq                                 \
	fts3aa-1.rq                                    \
	fts3aa-1.out                                   \
	fts3aa-2.rq                                    \
	fts3aa-2.out                                   \
	fts3ae-data.rq                                 \
	fts3ae-1.rq                                    \
	fts3ae-1.out


include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS +=                                          \
	tracker-test

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_srcdir)/tests/common                   \
	$(LIBTRACKER_CLIENT_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_CLIENT_LIBS)

tracker_test_SOURCES = tracker-test.c

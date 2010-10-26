include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-testcommon.la

AM_CPPFLAGS =                                          \
	$(BUILD)                                       \
	-DTEST_TEXT=\""$(top_srcdir)"/tests/libtracker-common/non-utf8.txt\" \
	$(LIBTRACKER_COMMON_CFLAGS)

LDADD =                                                \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_COMMON_LIBS)

libtracker_testcommon_la_SOURCES =                     \
	tracker-test-helpers.c                         \
	tracker-test-helpers.h


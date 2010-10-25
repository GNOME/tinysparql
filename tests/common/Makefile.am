include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-testcommon.la

INCLUDES = 									\
	$(BUILD)								\
	-DTEST_TEXT=\""$(top_srcdir)"/tests/libtracker-common/non-utf8.txt\"	\
	$(GLIB2_CFLAGS)

libtracker_testcommon_la_SOURCES = 						\
	tracker-test-helpers.c 							\
	tracker-test-helpers.h

libtracker_testcommon_la_LIBDADD = 						\
	$(BUILD_LIBS)                                                           \
	$(GLIB_LIBS)


include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-testcommon.la

INCLUDES = 				\
	-DTEST_TEXT=\""$(top_srcdir)"/tests/libtracker-common/non-utf8.txt\"	\
	-I.				\
	$(GLIB2_CFLAGS)

libtracker_testcommon_la_SOURCES = 	\
	tracker-test-helpers.c 		\
	tracker-test-helpers.h

libtracker_testcommon_la_LIBDADD = 	\
	$(GLIB2_LIBS)


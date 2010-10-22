include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-test

INCLUDES = 								\
	$(BUILD_CFLAGS)							\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_srcdir)/tests/common					\
	$(GLIB2_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)

tracker_test_SOURCES = 							\
	tracker-test.c

tracker_test_LDADD = 							\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)

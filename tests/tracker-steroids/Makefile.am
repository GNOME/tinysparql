include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-test

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_CFLAGS)

tracker_test_SOURCES = 							\
	tracker-test.c

tracker_test_LDADD = 							\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

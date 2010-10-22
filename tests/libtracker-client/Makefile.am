include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-test

INCLUDES = 								\
	$(BUILD_CFLAGS)							\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(GLIB2_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_CFLAGS)

tracker_test_SOURCES = 							\
	tracker-test.c

tracker_test_LDADD = 							\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)							\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)

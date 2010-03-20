include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-dbus 							\
	tracker-type-utils 						\
	tracker-file-utils						\
	tracker-utils

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
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)

tracker_dbus_SOURCES = 							\
	tracker-dbus-test.c						\
	mock-dbus.c							\
	mock-dbus-gproxy.c

tracker_dbus_LDADD =							\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

tracker_type_utils_SOURCES = \
	tracker-type-utils-test.c

tracker_type_utils_LDADD =						\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GOBJECT_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

tracker_file_utils_SOURCES = 						\
	tracker-file-utils-test.c

tracker_file_utils_LDADD =						\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

tracker_utils_SOURCES = 						\
	tracker-utils-test.c

tracker_utils_LDADD =							\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

EXTRA_DIST = non-utf8.txt

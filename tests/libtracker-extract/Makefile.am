include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	tracker-utils 							\
	tracker-xmp

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-sparql				\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_CFLAGS)

tracker_utils_SOURCES = \
	tracker-utils-test.c

tracker_utils_LDADD =							\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la   \
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GOBJECT_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

tracker_xmp_SOURCES = \
	tracker-xmp-test.c

tracker_xmp_LDADD =							\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la   \
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GOBJECT_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

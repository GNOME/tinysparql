include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-crawler tracker-thumbnailer-test

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/data"\"	\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	$(WARN_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(PANGO_CFLAGS)								\
	$(DBUS_CFLAGS)

tracker_crawler_SOURCES = 							\
	tracker-crawler-test.c

tracker_crawler_LDADD = 							\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-0.7.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

tracker_thumbnailer_test_SOURCES = 						\
	tracker-thumbnailer-test.c						\
	thumbnailer-mock.c							\
	empty-gobject.c

tracker_thumbnailer_test_LDADD = 						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-0.7.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

EXTRA_DIST = \
	data/empty-dir/.hidden \
	data/file1 \
	data/dir/empty-dir/.hidden \
	data/dir/file1 \
	data/dir/file2

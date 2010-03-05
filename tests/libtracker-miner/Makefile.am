include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-crawler

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-DTEST_DATA_DIR=\""$(abs_top_builddir)/tests/libtracker-miner/data"\"	\
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
	$(top_srcdir)/src/libtracker-miner/tracker-marshal.c			\
	$(top_srcdir)/src/libtracker-miner/tracker-marshal.h			\
	$(top_srcdir)/src/libtracker-miner/tracker-utils.c			\
	$(top_srcdir)/src/libtracker-miner/tracker-utils.h			\
	$(top_srcdir)/src/libtracker-miner/tracker-crawler.c			\
	tracker-crawler-test.c

tracker_crawler_LDADD = 								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

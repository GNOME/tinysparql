include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-metadata-utils \
	tracker-events

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/indexer-modules"\"	\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	-I$(top_builddir)/src/tracker-miner-fs					\
	$(WARN_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(PANGO_CFLAGS)								\
	$(DBUS_CFLAGS)								\
	$(RAPTOR_CFLAGS)


tracker_metadata_utils_SOURCES = 						\
	$(top_srcdir)/src/tracker-store/tracker-events.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-dbus.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-crawler.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-indexer.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-indexer-module.c		\
	$(top_srcdir)/src/tracker-miner-fs/tracker-marshal-main.c		\
	$(top_srcdir)/src/tracker-miner-fs/tracker-module-metadata-utils.c 	\
	$(top_srcdir)/src/tracker-miner-fs/tracker-module-file.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-module-iteratable.c		\
	$(top_srcdir)/src/tracker-miner-fs/tracker-monitor.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-processor.c			\
	$(top_srcdir)/src/tracker-miner-fs/tracker-removable-device.c		\
	tracker-metadata-utils-test.c 					

tracker_metadata_utils_LDADD =	                                        	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 			\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 			\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(top_builddir)/tests/common/libtracker-testcommon.la           	\
	$(top_builddir)/src/libinotify/libinotify.la				\
	$(DBUS_LIBS)								\
	$(PANGO_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GIO_LIBS)								\
	$(GCOV_LIBS)								\
	$(RAPTOR_LIBS)								\
	$(GLIB2_LIBS)							

tracker_events_SOURCES = 							\
	tracker-events-test.c							\
	$(top_srcdir)/src/tracker-store/tracker-events.c

tracker_events_LDADD = 								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)							

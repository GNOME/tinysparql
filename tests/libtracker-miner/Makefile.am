include $(top_srcdir)/Makefile.decl

# Include list of shared sources:
#  Defines:
#    $(libtracker_miner_monitor_sources)
#    $(libtracker_miner_monitor_headers)
#
# Headers and sources are split for the tests to build
# with make distcheck.
#
include $(top_srcdir)/src/libtracker-miner/Makefile-shared-sources.decl

noinst_LTLIBRARIES += libtracker-miner-tests.la

noinst_PROGRAMS += $(test_programs)

test_programs = \
	tracker-crawler-test                           \
	tracker-file-notifier-test		       \
	tracker-file-system-test		       \
	tracker-thumbnailer-test                       \
	tracker-monitor-test			       \
	tracker-priority-queue-test		       \
	tracker-task-pool-test			       \
	tracker-indexing-tree-test

AM_CPPFLAGS = \
	$(BUILD_VALACFLAGS) \
	-DTEST \
	-DLIBEXEC_PATH=\""$(libexecdir)"\" \
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/data"\" \
	-DTEST_MINERS_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/mock-miners"\" \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/tests/common \
	$(LIBTRACKER_MINER_CFLAGS)

LDADD = \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-private.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(LIBTRACKER_MINER_LIBS)

libtracker_miner_tests_la_VALAFLAGS =                  \
	--header tracker-miner-mock.h                  \
	--pkg glib-2.0                                 \
	--pkg gio-2.0                                  \
	--pkg gio-unix-2.0                             \
	--pkg posix				       \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(BUILD_VALAFLAGS)

libtracker_miner_tests_la_SOURCES =                    \
	tracker-miner-mock.vala			       \
	tracker-connection-mock.vala

libtracker_miner_tests_la_LIBADD = 		       \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)

tracker_crawler_test_SOURCES = \
	$(libtracker_miner_crawler_sources) \
	$(libtracker_miner_crawler_headers) \
	tracker-crawler-test.c

tracker_thumbnailer_test_SOURCES = \
	tracker-thumbnailer-test.c \
	thumbnailer-mock.c \
	empty-gobject.c

tracker_monitor_test_SOURCES =                         \
	tracker-monitor-test.c
if !ENABLE_GCOV
# If gcov is enabled, libtracker-miner exports all symbols and this is not needed.
tracker_monitor_test_SOURCES +=			       \
	$(libtracker_miner_monitor_sources)
endif

tracker_priority_queue_test_SOURCES = 		       \
	tracker-priority-queue-test.c

tracker_task_pool_test_SOURCES = 		       \
	tracker-task-pool-test.c

tracker_indexing_tree_test_SOURCES = \
	tracker-indexing-tree-test.c

tracker_file_system_test_SOURCES = \
	tracker-file-system-test.c

tracker_file_notifier_test_SOURCES =                   \
	$(libtracker_miner_monitor_sources)            \
	tracker-file-notifier-test.c

noinst_HEADERS =                                       \
	$(libtracker_miner_monitor_headers)            \
	tracker-miner-mock.h                           \
	thumbnailer-mock.h                             \
	miners-mock.h                                  \
	empty-gobject.h

BUILT_SOURCES += \
	$(libtracker_miner_file_system_sources) \
	$(libtracker_miner_file_system_headers) \
	libtracker_miner_tests_la_vala.stamp

EXTRA_DIST += \
	data/empty-dir/.hidden \
	data/file1 \
	data/dir/empty-dir/.hidden \
	data/dir/file1 \
	data/dir/file2 \
	mock-miners/mock-miner-1.desktop \
	mock-miners/mock-miner-2.desktop

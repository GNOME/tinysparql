include $(top_srcdir)/Makefile.decl

# Include list of shared sources:
#  Defines:
#    $(libtracker_miner_marshal_sources)
#    $(libtracker_miner_marshal_headers)
#    $(libtracker_miner_monitor_sources)
#    $(libtracker_miner_monitor_headers)
#
# Headers and sources are split for the tests to build
# with make distcheck.
#
include $(top_srcdir)/src/libtracker-miner/Makefile-shared-sources.decl

noinst_LTLIBRARIES = libtracker-miner-tests.la

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS +=                                          \
	tracker-crawler-test                           \
	tracker-miner-manager-test                     \
	tracker-password-provider-test                 \
	tracker-thumbnailer-test                       \
	tracker-monitor-test

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DTEST                                         \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/data"\" \
	-DTEST_MINERS_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/mock-miners"\" \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-I$(top_srcdir)/tests/common                   \
	$(LIBTRACKER_MINER_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_MINER_LIBS)

libtracker_miner_tests_la_VALAFLAGS =                  \
	--header tracker-miner-mock.h                  \
	--pkg glib-2.0                                 \
	$(BUILD_VALAFLAGS)

libtracker_miner_tests_la_SOURCES =                    \
	tracker-miner-mock.vala

tracker_crawler_test_SOURCES =                         \
	tracker-crawler-test.c

tracker_miner_manager_test_SOURCES =                   \
	tracker-miner-manager-test.c                   \
	miners-mock.c                                  \
	empty-gobject.c

tracker_miner_manager_test_LDADD =                     \
	libtracker-miner-tests.la                      \
	$(LDADD)

tracker_password_provider_test_SOURCES =               \
	tracker-password-provider-test.c

tracker_password_provider_test_LDADD =                 \
	libtracker-miner-tests.la                      \
	$(LDADD)

tracker_thumbnailer_test_SOURCES =                     \
	tracker-thumbnailer-test.c                     \
	thumbnailer-mock.c                             \
	empty-gobject.c

tracker_monitor_test_SOURCES =                         \
	$(libtracker_miner_marshal_sources)            \
	$(libtracker_miner_monitor_sources)            \
	tracker-monitor-test.c

noinst_HEADERS =                                       \
	$(libtracker_miner_marshal_headers)            \
	$(libtracker_miner_monitor_headers)            \
	tracker-miner-mock.h                           \
	thumbnailer-mock.h                             \
	miners-mock.h                                  \
	empty-gobject.h

BUILT_SOURCES =                                        \
	libtracker_miner_tests_la_vala.stamp

EXTRA_DIST =                                           \
	data/empty-dir/.hidden                         \
	data/file1                                     \
	data/dir/empty-dir/.hidden                     \
	data/dir/file1                                 \
	data/dir/file2                                 \
	mock-miners/mock-miner-1.desktop               \
	mock-miners/mock-miner-2.desktop

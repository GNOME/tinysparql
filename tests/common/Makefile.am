include $(top_srcdir)/Makefile.decl

# Don't install the test volume monitor into gio/modules or it will
# be loaded system-wide ... would be better to not install it at all,
# but it's used by the functional tests which operate on an installed
# Tracker.
moduledir = $(libdir)/tracker

noinst_LTLIBRARIES = \
	libtracker-testcommon.la

module_LTLIBRARIES = \
	libtracker-test-volume-monitor.la

libexec_PROGRAMS = \
	tracker-test-mounter-service

AM_CPPFLAGS =                                          \
	$(BUILD)                                       \
	-DTEST_TEXT=\""$(top_srcdir)"/tests/libtracker-common/non-utf8.txt\" \
	$(LIBTRACKER_COMMON_CFLAGS)

AM_VALAFLAGS = --pkg=gio-2.0 --pkg=gmodule-2.0 -g

LDADD =                                                \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_COMMON_LIBS)

libtracker_testcommon_la_SOURCES =                     \
	tracker-test-helpers.c                         \
	tracker-test-helpers.h

libtracker_test_volume_monitor_la_SOURCES =            \
	tracker-test-volume-monitor.vala

tracker_test_mounter_service_SOURCES =                 \
	tracker-test-mount-service.vala

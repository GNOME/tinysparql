include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS += $(test_programs)

check_PROGRAMS += tracker-parser

test_programs = \
	tracker-type-utils                             \
	tracker-dbus                                   \
	tracker-file-utils                             \
	tracker-utils				       \
	tracker-sched-test			       \
	tracker-date-time-test \
        tracker-parser-test

AM_CPPFLAGS =                                      \
	-DTOP_SRCDIR=\"$(abs_top_srcdir)\"             \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\"         \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_srcdir)/tests/common                   \
	$(LIBTRACKER_COMMON_CFLAGS)                    

LDADD =                                                \
	$(top_builddir)/tests/common/libtracker-testcommon.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_COMMON_LIBS)                      

tracker_dbus_SOURCES =                                 \
	tracker-dbus-test.c                            

tracker_type_utils_SOURCES = tracker-type-utils-test.c

tracker_file_utils_SOURCES = tracker-file-utils-test.c

tracker_utils_SOURCES = tracker-utils-test.c

tracker_sched_test_SOURCES = tracker-sched-test.c

tracker_date_time_test_SOURCES = tracker-date-time-test.c

tracker_parser_test_SOURCES = tracker-parser-test.c

tracker_parser_SOURCES = tracker-parser.c

EXTRA_DIST += \
	non-utf8.txt \
	meson.build

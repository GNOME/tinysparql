include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS +=                                          \
	tracker-type-utils                             \
	tracker-dbus                                   \
	tracker-file-utils                             \
	tracker-utils

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

EXTRA_DIST = non-utf8.txt encoding-detect.bin

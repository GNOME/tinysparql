include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS +=                                          \
	tracker-encoding                               \
	tracker-test-utils                             \
	tracker-test-xmp

AM_CPPFLAGS =                                          \
	-DTOP_SRCDIR=\"$(abs_top_srcdir)\"             \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\"         \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-I$(top_srcdir)/tests/common                   \
	$(LIBTRACKER_EXTRACT_CFLAGS)

LDADD =                                                \
	$(top_builddir)/tests/common/libtracker-testcommon.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_EXTRACT_LIBS)

tracker_encoding_SOURCES = tracker-encoding-test.c

tracker_test_utils_SOURCES = tracker-test-utils.c

tracker_test_xmp_SOURCES = tracker-test-xmp.c

EXTRA_DIST = encoding-detect.bin areas.xmp areas-with-contacts.xmp

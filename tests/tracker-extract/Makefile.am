include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules

noinst_PROGRAMS = $(TEST_PROGS)

# TEST_PROGS += 							\
# 	test-tracker-extract-mp3					\
# 	test-tracker-extract-png

# if HAVE_GSTREAMER
# TEST_PROGS += 							\
# 	test-tracker-extract-gstreamer
# endif

# if HAVE_LIBJPEG
# TEST_PROGS += 							\
# 	test-tracker-extract-jpeg
# endif

# if HAVE_LIBTIFF
# TEST_PROGS +=								\
# 	test-tracker-extract-tiff
# endif

INCLUDES = 								\
	$(BUILD_CFLAGS)							\
	-DMODULESDIR=\"$(modulesdir)\"					\
	-DTEST_DATA_DIR=\""$(abs_top_builddir)/tests/tracker-extract/data"\"\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(GLIB2_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_CFLAGS)

# TEST_UTILS_SOURCES = 							\
# 	tracker-extract-test-utils.c					\
# 	tracker-extract-test-utils.h					\
# 	tracker-extract-testsuite-generic.c				\
# 	tracker-extract-testsuite-generic.h				\
# 	tracker-extract-testsuite-mp3.c					\
# 	tracker-extract-testsuite-mp3.h					\
# 	tracker-extract-testsuite-jpeg.c				\
# 	tracker-extract-testsuite-jpeg.h				\
# 	tracker-extract-testsuite-tiff.c				\
# 	tracker-extract-testsuite-tiff.h				\
# 	tracker-extract-testsuite-png.c 				\
# 	tracker-extract-testsuite-png.h 				\
# 	tracker-extract-testsuite-avi.c					\
# 	tracker-extract-testsuite-avi.h

# test_tracker_extract_mp3_SOURCES = 					\
# 	tracker-extract-mp3-test.c					\
# 	$(TEST_UTILS_SOURCES)

# test_tracker_extract_mp3_LDADD =					\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
# 	$(GMODULE_LIBS)							\
# 	$(GCOV_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)

# test_tracker_extract_gstreamer_SOURCES = 				\
# 	tracker-extract-gstreamer-test.c				\
# 	$(TEST_UTILS_SOURCES)

# test_tracker_extract_gstreamer_LDADD =				\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
# 	$(GMODULE_LIBS)							\
# 	$(GCOV_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	$(GSTREAMER_LIBS)

# test_tracker_extract_jpeg_SOURCES = 					\
# 	tracker-extract-jpeg-test.c					\
# 	$(TEST_UTILS_SOURCES)

# test_tracker_extract_jpeg_LDADD =					\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GCOV_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	$(LIBJPEG_LIBS)							\
# 	$(LIBEXIF_LIBS)							\
# 	$(EXEMPI_LIBS)

# test_tracker_extract_tiff_SOURCES = 					\
# 	tracker-extract-tiff-test.c					\
# 	$(TEST_UTILS_SOURCES)

# test_tracker_extract_tiff_LDADD =					\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	$(GCOV_LIBS)							\
# 	$(LIBTIFF_LIBS)							\
# 	$(EXEMPI_LIBS)

# test_tracker_extract_png_SOURCES = 					\
# 	tracker-extract-png-test.c					\
# 	$(TEST_UTILS_SOURCES)

# test_tracker_extract_png_LDADD =					\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
# 	$(GMODULE_LIBS)							\
# 	$(GTHREAD_LIBS)							\
# 	$(GLIB2_LIBS)							\
# 	$(GCOV_LIBS)							\
# 	$(LIBPNG_LIBS)							\
# 	$(EXEMPI_LIBS)


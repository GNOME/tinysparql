include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_UTILS_SOURCES = tracker-extract-test-utils.c			\
		     tracker-extract-testsuite-generic.c		\
		     tracker-extract-testsuite-mp3.c



TEST_PROGS += 								\
	test-tracker-extract-mp3					\
	test-tracker-extract-gstreamer

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTEST_DATA_DIR=\""$(abs_top_builddir)/tests/tracker-extract/data"\"\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(DBUS_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(GLIB2_CFLAGS)

test_tracker_extract_mp3_SOURCES = 					\
	tracker-extract-mp3-test.c					\
	$(TEST_UTILS_SOURCES)

test_tracker_extract_mp3_LDADD =					\
	$(top_builddir)/src/tracker-extract/libextract-mp3.la   	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

test_tracker_extract_gstreamer_SOURCES = 				\
	tracker-extract-gstreamer-test.c				\
	$(TEST_UTILS_SOURCES)

test_tracker_extract_gstreamer_LDADD =					\
	$(top_builddir)/src/tracker-extract/libextract-gstreamer.la   	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GSTREAMER_LIBS)
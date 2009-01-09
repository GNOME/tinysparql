include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 								\
	test-tracker-extract-mp3

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTEST_DATA_DIR=\""$(top_builddir)/tests/tracker-extract/data"\"\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(DBUS_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(GLIB2_CFLAGS)

test_tracker_extract_mp3_SOURCES = 					\
	tracker-extract-mp3-test.c

test_tracker_extract_mp3_LDADD =					\
	$(top_builddir)/src/tracker-extract/libextract-mp3.la   	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

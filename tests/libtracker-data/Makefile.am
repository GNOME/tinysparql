include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-turtle

INCLUDES = 					\
	-DG_LOG_DOMAIN=\"Tracker\"		\
	-DTRACKER_COMPILATION			\
	-I$(top_srcdir)/src			\
	-I$(top_srcdir)/tests/common		\
	$(WARN_CFLAGS)				\
	$(RAPTOR_CFLAGS)			\
	$(GLIB2_CFLAGS)				\
	$(GCOV_CFLAGS)				\
	$(GMODULE_CFLAGS)			\
	$(GTHREAD_CFLAGS)			\
	$(DBUS_CFLAGS)				\
	$(SQLITE3_CFLAGS)			\
	$(QDBM_CFLAGS)

tracker_turtle_SOURCES = 			\
	tracker-turtle-test.c

tracker_turtle_LDADD =							\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(RAPTOR_LIBS)							\
	$(QDBM_LIBS)							\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							

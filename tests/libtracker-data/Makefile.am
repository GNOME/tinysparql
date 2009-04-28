include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-turtle \
	tracker-data-metadata \
	tracker-field-data

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
	$(DBUS_CFLAGS)				

tracker_turtle_SOURCES = 			\
	tracker-turtle-test.c

tracker_turtle_LDADD =							\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(RAPTOR_LIBS)							\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							

tracker_data_metadata_SOURCES =			\
	tracker-data-metadata-test.c

tracker_data_metadata_LDADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(top_builddir)/tests/common/libtracker-testcommon.la 		\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							

tracker_field_data_SOURCES = 			\
	tracker-field-data-test.c

tracker_field_data_LDADD =						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 		\
	$(GCOV_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							

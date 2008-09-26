include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_SETUP) $(TEST_PROGS)

TEST_SETUP  = $(generated_files)
TEST_PROGS += tracker-metadata-utils

INCLUDES = 								\
	-g -DTEST							\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker/indexer-modules"\"	\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"				\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/tests/common					\
	$(DBUS_CFLAGS)							\
	$(PANGO_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)			

tracker_metadata_utils_SOURCES = 					\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-indexer.c						\
	tracker-indexer.h						\
	tracker-indexer-db.c						\
	tracker-indexer-db.h						\
	tracker-indexer-module.c					\
	tracker-indexer-module.h					\
	tracker-marshal-main.c						\
	tracker-metadata.c						\
	tracker-metadata.h						\
	tracker-metadata-utils.c 					\
	tracker-metadata-utils.h 					\
	tracker-metadata-utils-test.c 					\
	tracker-module.h

tracker_metadata_utils_LDADD =	                                        \
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/tests/common/libtracker-testcommon.la           \
	$(DBUS_LIBS)							\
	$(PANGO_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)							

#
# We don't add these to CLEANFILES because they are created from
# src/tracker-indexer and will be overwritten when we rebuild from
# there. 
#
marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

dbus_sources = 								\
	tracker-indexer-glue.h

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(marshal_sources)

EXTRA_DIST = 								\
	$(BUILT_SOURCES)

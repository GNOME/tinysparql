include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-metadata-utils

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker/indexer-modules"\"		\
	-DLIBDIR=\""$(libdir)"\"						\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-indexer					\
	$(DBUS_CFLAGS)								\
	$(PANGO_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(RAPTOR_CFLAGS)							\
	$(GLIB2_CFLAGS)

tracker_metadata_utils_SOURCES = 						\
	$(top_srcdir)/src/tracker-indexer/tracker-dbus.c			\
	$(top_srcdir)/src/tracker-indexer/tracker-indexer.c			\
	$(top_srcdir)/src/tracker-indexer/tracker-indexer-module.c		\
	$(top_srcdir)/src/tracker-indexer/tracker-marshal-main.c		\
	$(top_srcdir)/src/tracker-indexer/tracker-module-metadata.c		\
	$(top_srcdir)/src/tracker-indexer/tracker-module-metadata.h		\
	$(top_srcdir)/src/tracker-indexer/tracker-module-metadata-utils.c 	\
	$(top_srcdir)/src/tracker-indexer/tracker-module-file.c			\
	$(top_srcdir)/src/tracker-indexer/tracker-module-iteratable.c		\
	$(top_srcdir)/src/tracker-indexer/tracker-removable-device.c		\
	tracker-metadata-utils-test.c 					

tracker_metadata_utils_LDADD =	                                        	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 			\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 			\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(top_builddir)/tests/common/libtracker-testcommon.la           	\
	$(DBUS_LIBS)								\
	$(PANGO_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GIO_LIBS)								\
	$(RAPTOR_LIBS)								\
	$(GLIB2_LIBS)							

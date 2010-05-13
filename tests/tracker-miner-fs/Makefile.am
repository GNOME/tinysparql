include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += tracker-metadata-utils

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/indexer-modules"\"	\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	-I$(top_builddir)/src/miners/fs						\
	$(WARN_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(DBUS_CFLAGS)

tracker_metadata_utils_SOURCES = 						\
	$(top_srcdir)/src/miners/fs/tracker-config.c				\
	$(top_builddir)/src/miners/fs/tracker-marshal.c				\
	tracker-metadata-utils-test.c

tracker_metadata_utils_LDADD =	                                        	\
	$(top_builddir)/src/libtracker-data/libtracker-data.la 			\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 			\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/tests/common/libtracker-testcommon.la           	\
	$(DBUS_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GIO_LIBS)								\
	$(GCOV_LIBS)								\
	$(GLIB2_LIBS)


if HAVE_LIBINOTIFY
tracker_metadata_utils_LDADD +=							\
	$(top_builddir)/src/libinotify/libinotify.la
endif #HAVE_LIBINOTIFY

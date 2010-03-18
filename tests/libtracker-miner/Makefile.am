include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 									\
	tracker-crawler 							\
	tracker-miner-manager							\
	tracker-password-provider-test						\
	tracker-thumbnailer-test

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/data"\"	\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	$(WARN_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GIO_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(PANGO_CFLAGS)								\
	$(DBUS_CFLAGS)

BUILT_SOURCES = libtracker-miner-testing.vala.stamp

tracker_crawler_SOURCES = 							\
	tracker-crawler-test.c

tracker_crawler_LDADD = 							\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-0.7.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GIO_LIBS)								\
	$(GLIB2_LIBS)

tracker_thumbnailer_test_SOURCES = 						\
	tracker-thumbnailer-test.c						\
	thumbnailer-mock.h							\
	thumbnailer-mock.c							\
	empty-gobject.h								\
	empty-gobject.c

tracker_thumbnailer_test_LDADD = 						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-0.7.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

tracker_miner_manager_VALASOURCES =						\
	tracker-miner-mock.vala

tracker_miner_manager_SOURCES = 						\
	tracker-miner-manager-test.c						\
	miners-mock.c								\
	miners-mock.h								\
	$(tracker_miner_manager_VALASOURCES:.vala=.c)				\
	$(tracker_miner_manager_VALASOURCES:.vala=.h)				\
	empty-gobject.h								\
	empty-gobject.c

tracker_miner_manager_LDADD = 							\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-0.7.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

tracker_password_provider_test_SOURCES =      					\
	tracker-password-provider-test.c

tracker_password_provider_test_LDADD =        					\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(DBUS_LIBS)								\
	$(GCOV_LIBS)								\
	$(GMODULE_LIBS)								\
	$(GTHREAD_LIBS)								\
	$(GLIB2_LIBS)

libtracker-miner-testing.vala.stamp: $(tracker_miner_manager_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) --pkg glib-2.0 -C $(VALAFLAGS) -H $(<:.vala=.h) $<
	touch $@

MAINTAINERCLEANFILES =					\
	libtracker-miner-testing.vala.stamp		\
	$(tracker_miner_manager_VALASOURCES:.vala=.c)	

EXTRA_DIST = $(tracker_miner_manager_VALASOURCES) \
	libtracker-miner-testing.vala.stamp  \
	data/empty-dir/.hidden \
	data/file1 \
	data/dir/empty-dir/.hidden \
	data/dir/file1 \
	data/dir/file2 \
	mock-miners/mock-miner-1.desktop \
	mock-miners/mock-miner-2.desktop

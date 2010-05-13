include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS +=									\
	tracker-crawler-test							\
	tracker-miner-manager-test						\
	tracker-password-provider-test						\
	tracker-thumbnailer-test

INCLUDES =									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DPKGLIBDIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)"\"		\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-DTEST_DATA_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/data"\"	\
	-DTEST_MINERS_DIR=\""$(abs_top_srcdir)/tests/libtracker-miner/mock-miners"\"	\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	-I$(top_builddir)/src							\
	-I$(top_builddir)/src/libtracker-client					\
	$(WARN_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GIO_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(DBUS_CFLAGS)

LIBS =										\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la		\
	$(DBUS_LIBS)                                                            \
	$(GCOV_LIBS)                                                            \
	$(GMODULE_LIBS)                                                         \
	$(GTHREAD_LIBS)                                                         \
	$(GIO_LIBS)                                                             \
	$(GLIB2_LIBS)

tracker_crawler_test_SOURCES =							\
	tracker-crawler-test.c

tracker_thumbnailer_test_SOURCES =						\
	tracker-thumbnailer-test.c						\
	thumbnailer-mock.h							\
	thumbnailer-mock.c							\
	empty-gobject.h								\
	empty-gobject.c

tracker_miner_manager_test_VALASOURCES =					\
	tracker-miner-mock.vala

tracker_miner_manager_test_SOURCES =						\
	tracker-miner-manager-test.c						\
	miners-mock.c								\
	miners-mock.h								\
	$(tracker_miner_manager_test_VALASOURCES:.vala=.c)			\
	$(tracker_miner_manager_test_VALASOURCES:.vala=.h)			\
	empty-gobject.h								\
	empty-gobject.c

tracker_password_provider_test_SOURCES =					\
	tracker-password-provider-test.c

libtracker-miner-test.vala.stamp: $(tracker_miner_manager_test_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) --pkg glib-2.0 -C $(VALAFLAGS) -H $(<:.vala=.h) $<
	touch $@

BUILT_SOURCES = libtracker-miner-test.vala.stamp

MAINTAINERCLEANFILES =								\
	libtracker-miner-test.vala.stamp					\
	$(tracker_miner_manager_test_VALASOURCES:.vala=.c)

EXTRA_DIST = $(tracker_miner_manager_test_VALASOURCES)				\
	libtracker-miner-test.vala.stamp					\
	data/empty-dir/.hidden							\
	data/file1								\
	data/dir/empty-dir/.hidden						\
	data/dir/file1								\
	data/dir/file2								\
	mock-miners/mock-miner-1.desktop					\
	mock-miners/mock-miner-2.desktop

include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-Wall								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DPKGLIBDIR=\""$(libdir)/tracker"\"				\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(WARN_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(LIBGRSS_CFLAGS)						\
	$(GCOV_CFLAGS)

libexec_PROGRAMS = tracker-miner-rss

tracker_miner_rss_SOURCES =						\
	tracker-main.c							\
	tracker-miner-rss.h						\
	tracker-miner-rss.c

tracker_miner_rss_LDADD =						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBGRSS_LIBS)							\
	$(DBUS_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GTHREAD_LIBS)

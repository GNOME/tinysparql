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
	$(GMODULE_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(WARN_CFLAGS)							\
	$(LIBGRSS_CFLAGS)

libexec_PROGRAMS = tracker-miner-rss

tracker_miner_rss_SOURCES =						\
	tracker-main.c						\
	tracker-miner-rss.h						\
	tracker-miner-rss.c

tracker_miner_rss_LDADD =						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(GLIB2_LIBS)						\
	$(DBUS_LIBS)						\
	$(LIBGRSS_LIBS)

CLEANFILES = $(BUILT_SOURCES)

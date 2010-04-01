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
	$(WARN_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(MINER_TWITTER_CFLAGS)						\
	$(GCOV_CFLAGS)

VALAFLAGS =								\
	--pkg gio-2.0							\
	--pkg twitter-glib-1.0						\
	--pkg posix							\
	--thread

libexec_PROGRAMS = tracker-miner-twitter

tracker_miner_twitter_VALASOURCES =					\
	tracker-miner-twitter.vala					\
	query-queue.vala						\
	$(top_srcdir)/src/libtracker-client/tracker-client.vapi

tracker_miner_twitter_SOURCES =						\
	$(tracker_miner_twitter_VALASOURCES:.vala=.c)

tracker_miner_twitter_LDADD =						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)							\
	$(MINER_TWITTER_LIBS)							\
	-lz								\
	-lm

vapi_sources =								\
	$(top_srcdir)/src/libtracker-miner/tracker-miner-@TRACKER_API_VERSION@.vapi

tracker-miner-twitter.vala.stamp: $(tracker_miner_twitter_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) $^
	touch $@


BUILT_SOURCES = tracker-miner-twitter.vala.stamp

MAINTAINERCLEANFILES =							\
	tracker-miner-twitter.vala.stamp				\
	$(tracker_miner_twitter_VALASOURCES:.vala=.c)

EXTRA_DIST =								\
	$(tracker_miner_twitter_VALASOURCES)				\
	tracker-miner-twitter.vala.stamp

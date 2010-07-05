include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

# TEST_PROGS += tracker-events

INCLUDES = 									\
	-DTEST									\
	-DG_LOG_DOMAIN=\"Tracker\"						\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"					\
	-DTRACKER_COMPILATION							\
	-I$(top_srcdir)/src							\
	-I$(top_srcdir)/tests/common						\
	-I$(top_builddir)/src/tracker-store					\
	-I$(top_builddir)/src/tracker-miner-fs					\
	$(WARN_CFLAGS)								\
	$(GLIB2_CFLAGS)								\
	$(GCOV_CFLAGS)								\
	$(GMODULE_CFLAGS)							\
	$(GTHREAD_CFLAGS)							\
	$(DBUS_CFLAGS)

# tracker_events_SOURCES = 							\
# 	$(top_srcdir)/src/tracker-store/tracker-events.c			\
# 	tracker-events-test.c

# tracker_events_LDADD = 							\
# 	$(top_builddir)/src/libtracker-common/libtracker-common.la 		\
# 	$(DBUS_LIBS)								\
# 	$(GCOV_LIBS)								\
# 	$(GMODULE_LIBS)								\
# 	$(GTHREAD_LIBS)								\
# 	$(GLIB2_LIBS)							

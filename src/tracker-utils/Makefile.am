include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

libs = 									\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

bin_PROGRAMS = 								\
	tracker-import							\
	tracker-info							\
	tracker-search 							\
	tracker-sparql							\
	tracker-stats							\
	tracker-status							\
	tracker-tag

tracker_search_SOURCES = tracker-search.c
tracker_search_LDADD = $(libs)

tracker_stats_SOURCES = tracker-stats.c
tracker_stats_LDADD = $(libs)

tracker_tag_SOURCES = tracker-tag.c
tracker_tag_LDADD = $(libs)

tracker_status_SOURCES = tracker-status.c
tracker_status_LDADD = $(libs)

tracker_info_SOURCES = tracker-info.c
tracker_info_LDADD = $(libs)

tracker_sparql_SOURCES = tracker-sparql.c
tracker_sparql_LDADD = $(libs)

tracker_import_SOURCES = tracker-import.c
tracker_import_LDADD = $(libs)

dbus_sources = 								\
	tracker-status-client.h

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

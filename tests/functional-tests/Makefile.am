include $(top_srcdir)/Makefile.decl

SUBDIRS = data

INCLUDES =					\
	-I$(top_srcdir)/src/libtracker-sparql	\
	-I$(top_srcdir)/src/libtracker-bus	\
	-I$(top_builddir)/src/libtracker-sparql	\
	-I$(top_builddir)/src/libtracker-bus	\
	$(WARN_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GIO_CFLAGS)				\
	$(GTHREAD_CFLAGS)

configdir = $(datadir)/tracker-tests

config_SCRIPTS =			\
	01-insertion.py			\
	02-metacontacts.py		\
	03-fts-functions.py		\
	04-group-concat.py		\
	05-coalesce.py			\
	06-distance.py			\
	07-graph.py			\
	08-unique-insertions.py		\
	09-concurrent-query.py		\
	backup-restore-tc.py		\
	bootup-tc.py			\
	configuration.py		\
	metadata_extraction_tc.py	\
	mass-storage-mode.py		\
	fts-tc.py			\
	miner-basic-ops.py		\
	performance-tc.py		\
	tests.xml			\
	virtual-files-tc.py		\
	writeback-tc.py

bashscriptsdir = $(datadir)/tracker-tests

bashscripts_in_files =				\
	force-sqlite-misused.sh.in		\
	force-sqlite-misused-batch.sh.in	\
	helper-test-data.sh.in

bashscripts_SCRIPTS = 				\
	$(bashscripts_in_files:.sh.in=.sh)

%.sh: %.sh.in
	@sed -e "s|@topsrcdir[@]|${top_srcdir}|" \
	     -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

noinst_PROGRAMS = busy-handling-test bus-query-test bus-update-test

busy_handling_test_VALASOURCES = busy-handling-test.vala

busy_handling_test_SOURCES = \
	busy-handling-test.vala.stamp \
	$(busy_handling_test_VALASOURCES:.vala=.c)

busy-handling-test.vala.stamp: $(busy_handling_test_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

busy_handling_test_LDADD = \
	$(GIO_LIBS) \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)

BUILT_SOURCES = 				\
	busy-handling-test.vala.stamp

bus_query_test_VALASOURCES = shared-query-test.vala bus-query-test.vala

bus_query_test_SOURCES = \
	bus-query-test.vala.stamp \
	$(bus_query_test_VALASOURCES:.vala=.c)

bus_update_test_VALASOURCES = shared-update-test.vala bus-update-test.vala

bus_update_test_SOURCES = \
	bus-update-test.vala.stamp \
	$(bus_update_test_VALASOURCES:.vala=.c)

bus_vapi_sources = \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-bus/tracker-bus-$(TRACKER_API_VERSION).vapi

bus-query-test.vala.stamp: $(bus_query_test_VALASOURCES) $(bus_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

bus-update-test.vala.stamp: $(bus_update_test_VALASOURCES) $(bus_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

bus_query_test_LDADD = \
	$(top_builddir)/src/libtracker-bus/libtracker-bus.la \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(GIO_LIBS) \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)

bus_update_test_LDADD = $(bus_query_test_LDADD)

BUILT_SOURCES = 				\
	busy-handling-test.vala.stamp \
	bus-query-test.vala.stamp \
	bus-query-test.vala.stamp 		\
	bus-update-test.vala.stamp


EXTRA_DIST = 					\
	$(config_SCRIPTS) 			\
	$(bashscripts_SCRIPTS)			\
	$(bashscripts_in_files)			\
	$(BUILT_SOURCES)			\
	$(busy_handling_test_VALASOURCES)	\
	$(direct_query_test_VALASOURCES)	\
	$(default_update_test_VALASOURCES)	\
	$(bus_query_test_VALASOURCES)		\
	$(bus_update_test_VALASOURCES)

CLEANFILES = $(bashscripts_SCRIPTS)

MAINTAINERCLEANFILES =					\
	$(BUILT_SOURCES)				\
	$(busy_handling_test_VALASOURCES:.vala=.c)	\
	$(bus_query_test_VALASOURCES:.vala=.c)		\
	$(bus_update_test_VALASOURCES:.vala=.c)


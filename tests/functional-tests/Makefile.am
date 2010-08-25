include $(top_srcdir)/Makefile.decl

SUBDIRS=common test-configurations test-extraction-data test-writeback-data ttl unittest2

INCLUDES =					\
	-I$(top_srcdir)/src/libtracker-sparql	\
	-I$(top_srcdir)/src/libtracker-direct	\
	-I$(top_srcdir)/src/libtracker-bus	\
	-I$(top_builddir)/src/libtracker-sparql	\
	-I$(top_builddir)/src/libtracker-direct	\
	-I$(top_builddir)/src/libtracker-bus	\
	$(WARN_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GIO_CFLAGS)				\
	$(GTHREAD_CFLAGS)

configdir = $(datadir)/tracker-tests

config_SCRIPTS =			\
	__init__.py			\
	tests.xml			\
	$(slow_tests)			\
	$(standard_tests)

standard_tests = 			\
	01-insertion.py
if HAVE_TRACKER_FTS
standard_tests += 03-fts-functions.py
endif
standard_tests +=			\
	04-group-concat.py		\
	05-coalesce.py			\
	06-distance.py			\
	07-graph.py			\
	08-unique-insertions.py		\
	09-concurrent-query.py		\
	14-signals.py			\
	200-backup-restore.py		\
	300-miner-basic-ops.py
if HAVE_TRACKER_FTS
standard_tests += 310-fts-indexing.py
endif
standard_tests +=			\
	400-extractor.py		\
	500-writeback.py		\
	mass-storage-mode.py		\
	performance-tc-modified.py	\
	virtual-files-tc.py

slow_tests = 				\
	10-sqlite-misused.py		\
	11-sqlite-batch-misused.py	\
	12-transactions.py		\
	13-threaded-store.py

tests.xml:
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
	fi;
	./create-tests-xml.py $(standard_tests) > tests.xml

10-sqlite-misused.py: ttl/gen-test-data.stamp
11-sqlite-misused-batch.py: ttl/gen-test-data.stamp
13-busy-store.py: ttl/gen-test-data.stamp

ttl/gen-test-data.stamp:
	cd ttl && $(MAKE) $(AM_MAKEFLAGS)

noinst_PROGRAMS = busy-handling-test \
	direct-query-test \
	bus-query-test \
	default-update-test \
	bus-update-test \
	class-signal-test \
	class-signal-performance-test

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

direct_query_test_VALASOURCES = shared-query-test.vala direct-query-test.vala

direct_query_test_SOURCES = \
	direct-query-test.vala.stamp \
	$(direct_query_test_VALASOURCES:.vala=.c)

default_update_test_VALASOURCES = shared-update-test.vala default-update-test.vala

default_update_test_SOURCES = \
	default-update-test.vala.stamp \
	$(default_update_test_VALASOURCES:.vala=.c)

bus_query_test_VALASOURCES = shared-query-test.vala bus-query-test.vala

bus_query_test_SOURCES = \
	bus-query-test.vala.stamp \
	$(bus_query_test_VALASOURCES:.vala=.c)

bus_update_test_VALASOURCES = shared-update-test.vala bus-update-test.vala

bus_update_test_SOURCES = \
	bus-update-test.vala.stamp \
	$(bus_update_test_VALASOURCES:.vala=.c)

direct_vapi_sources = \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-direct/tracker-direct.vapi

direct-query-test.vala.stamp: $(direct_query_test_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 $^
	$(AM_V_GEN)touch $@

direct_query_test_LDADD = \
	$(top_builddir)/src/libtracker-direct/libtracker-direct.la \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(GIO_LIBS) \
	$(GLIB2_LIBS)

default-update-test.vala.stamp: $(default_update_test_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 $^
	$(AM_V_GEN)touch $@

default_update_test_LDADD = $(direct_query_test_LDADD)

bus_vapi_sources = \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-bus/tracker-bus.vapi

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

class_signal_test_VALASOURCES = class-signal-test.vala

class_signal_test_SOURCES = \
	class-signal-test.vala.stamp \
	$(class_signal_test_VALASOURCES:.vala=.c)

class-signal-test.vala.stamp: $(class_signal_test_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

class_signal_test_LDADD = $(direct_query_test_LDADD)

class_signal_performance_test_VALASOURCES = class-signal-performance-test.vala

class_signal_performance_test_SOURCES = \
	class-signal-performance-test.vala.stamp \
	$(class_signal_performance_test_VALASOURCES:.vala=.c)

class-signal-performance-test.vala.stamp: $(class_signal_performance_test_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

class_signal_performance_test_LDADD = $(direct_query_test_LDADD)

BUILT_SOURCES = 				\
	busy-handling-test.vala.stamp 		\
	class-signal-test.vala.stamp 		\
	class-signal-performance-test.vala.stamp \
	direct-query-test.vala.stamp 		\
	bus-query-test.vala.stamp 		\
	default-update-test.vala.stamp 		\
	bus-update-test.vala.stamp


functional-test: ${standard_tests}
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
		ENV_COMMAND_WRAPPER=meego-run ;\
	else \
		ENV_COMMAND_WRAPPER="" ; \
	fi ; \
	for testfile in ${standard_tests} ; do \
	  echo "Trying with" $$ENV_COMMAND_WRAPPER $$testfile ; \
	  test ! -x $$testfile  || $$ENV_COMMAND_WRAPPER ./$$testfile > /dev/null; \
	 done

functional-test-slow: ${slow_tests}
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
		ENV_COMMAND_WRAPPER=meego-run ;\
	else \
		ENV_COMMAND_WRAPPER="" ; \
	fi ; \
	@for test in ${slow_tests} ; do \
	  echo "Trying with" $$ENV_COMMAND_WRAPPER $$test ; \
	  test ! -x $$test  || $$ENV_COMMAND_WRAPPER ./$$test > /dev/null; \
	 done

EXTRA_DIST = 					\
	$(config_SCRIPTS) 			\
	$(BUILT_SOURCES)			\
	$(busy_handling_test_VALASOURCES)	\
	$(class_signal_test_VALASOURCES)	\
	$(class_signal_performance_test_VALASOURCES) \
	$(direct_query_test_VALASOURCES)	\
	$(default_update_test_VALASOURCES)	\
	$(bus_query_test_VALASOURCES)		\
	$(bus_update_test_VALASOURCES)

MAINTAINERCLEANFILES =					\
	$(BUILT_SOURCES)				\
	$(busy_handling_test_VALASOURCES:.vala=.c)	\
	$(class_signal_test_VALASOURCES:.vala=.c)	\
	$(class_signal_performance_test_VALASOURCES:.vala=.c) \
	$(bus_query_test_VALASOURCES:.vala=.c)		\
	$(bus_update_test_VALASOURCES:.vala=.c)		\
	$(direct_query_test_VALASOURCES:.vala=.c)	\
	$(default_update_test_VALASOURCES:.vala=.c)


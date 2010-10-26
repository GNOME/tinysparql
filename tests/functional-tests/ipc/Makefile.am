include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS =                                      \
	test-busy-handling                             \
	test-direct-query                              \
	test-bus-query                                 \
	test-default-update                            \
	test-bus-update                                \
	test-class-signal                              \
	test-class-signal-performance                  \
	test-update-array-performance

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-direct/libtracker-direct.la \
	$(top_builddir)/src/libtracker-bus/libtracker-bus.la \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

test_busy_handling_VALASOURCES = test-busy-handling.vala

test_busy_handling_SOURCES =                           \
	test-busy-handling.vala.stamp                  \
	$(test_busy_handling_VALASOURCES:.vala=.c)

test-busy-handling.vala.stamp: $(test_busy_handling_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

test_direct_query_VALASOURCES = test-shared-query.vala test-direct-query.vala

test_direct_query_SOURCES =                            \
	test-direct-query.vala.stamp                   \
	$(test_direct_query_VALASOURCES:.vala=.c)

test_default_update_VALASOURCES = test-shared-update.vala test-default-update.vala

test_default_update_SOURCES =                          \
	test-default-update.vala.stamp                 \
	$(test_default_update_VALASOURCES:.vala=.c)

test_bus_query_VALASOURCES = test-shared-query.vala test-bus-query.vala

test_update_array_performance_SOURCES = test-update-array-performance.c

test_bus_query_SOURCES =                               \
	test-bus-query.vala.stamp                      \
	$(test_bus_query_VALASOURCES:.vala=.c)

test_bus_update_VALASOURCES = test-shared-update.vala test-bus-update.vala

test_bus_update_SOURCES =                              \
	test-bus-update.vala.stamp                     \
	$(test_bus_update_VALASOURCES:.vala=.c)

direct_vapi_sources =                                  \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-direct/tracker-direct.vapi

test-direct-query.vala.stamp: $(test_direct_query_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 $^
	$(AM_V_GEN)touch $@

test-default-update.vala.stamp: $(test_default_update_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 $^
	$(AM_V_GEN)touch $@

bus_vapi_sources =                                     \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-bus/tracker-bus.vapi

test-bus-query.vala.stamp: $(test_bus_query_VALASOURCES) $(bus_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

test-bus-update.vala.stamp: $(test_bus_update_VALASOURCES) $(bus_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

test_class_signal_VALASOURCES = test-class-signal.vala

test_class_signal_SOURCES =                            \
	test-class-signal.vala.stamp                   \
	$(test_class_signal_VALASOURCES:.vala=.c)

test-class-signal.vala.stamp: $(test_class_signal_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

test_class_signal_performance_VALASOURCES = test-class-signal-performance.vala

test_class_signal_performance_SOURCES =                \
	test-class-signal-performance.vala.stamp       \
	$(test_class_signal_performance_VALASOURCES:.vala=.c)

test-class-signal-performance.vala.stamp: $(test_class_signal_performance_VALASOURCES) $(direct_vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES =                                        \
	test-busy-handling.vala.stamp                  \
	test-class-signal.vala.stamp                   \
	test-class-signal-performance.vala.stamp       \
	test-direct-query.vala.stamp                   \
	test-bus-query.vala.stamp                      \
	test-default-update.vala.stamp                 \
	test-bus-update.vala.stamp

EXTRA_DIST =                                           \
	$(BUILT_SOURCES)                               \
	$(test_busy_handling_VALASOURCES)              \
	$(test_class_signal_VALASOURCES)               \
	$(test_class_signal_performance_VALASOURCES)   \
	$(test_direct_query_VALASOURCES)               \
	$(test_default_update_VALASOURCES)             \
	$(test_bus_query_VALASOURCES)                  \
	$(test_bus_update_VALASOURCES)

MAINTAINERCLEANFILES =                                 \
	$(BUILT_SOURCES)                               \
	$(test_busy_handling_VALASOURCES:.vala=.c)     \
	$(test_class_signal_VALASOURCES:.vala=.c)      \
	$(test_class_signal_performance_VALASOURCES:.vala=.c) \
	$(test_bus_query_VALASOURCES:.vala=.c)         \
	$(test_bus_update_VALASOURCES:.vala=.c)        \
	$(test_direct_query_VALASOURCES:.vala=.c)      \
	$(test_default_update_VALASOURCES:.vala=.c)

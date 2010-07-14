include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_builddir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)

lib_LTLIBRARIES = libtracker-bus-@TRACKER_API_VERSION@.la

libtracker_busincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-bus

libtracker_bus_la_VALASOURCES = \
	tracker-bus.vala

libtracker_bus_@TRACKER_API_VERSION@_la_SOURCES = 	\
	libtracker-bus.vala.stamp			\
	$(libtracker_bus_la_VALASOURCES:.vala=.c)

libtracker_bus_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-shared -fPIC -module -avoid-version

#	-export-symbols-regex '^tracker_.*'

libtracker_businclude_HEADERS = 			\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	tracker-bus.h

# Vala sources
vapi_sources =						\
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi	\
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA = \
	tracker-bus-$(TRACKER_API_VERSION).vapi

libtracker-bus.vala.stamp: $(libtracker_bus_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-bus.h --vapi tracker-bus-$(TRACKER_API_VERSION).vapi $^
	$(AM_V_GEN)touch $@

libtracker_bus_@TRACKER_API_VERSION@_la_LIBADD = 						\
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)

BUILT_SOURCES = 					\
	libtracker-bus.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	$(libtracker_bus_la_VALASOURCES:.vala=.c) \
	libtracker-bus.vala.stamp			\
	tracker-bus.h

EXTRA_DIST = 						\
	$(libtracker_bus_la_VALASOURCES) \
	libtracker-bus.vala.stamp

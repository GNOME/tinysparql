include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

INCLUDES =								\
	$(BUILD_CFLAGS)							\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	$(LIBTRACKER_BUS_CFLAGS)

modules_LTLIBRARIES = libtracker-bus.la

libtracker_bus_la_VALASOURCES = 		\
	tracker-bus.vala			\
	tracker-array-cursor.vala

libtracker_bus_la_SOURCES = 	\
	libtracker-bus.vala.stamp			\
	$(libtracker_bus_la_VALASOURCES:.vala=.c)	\
	tracker-bus-fd-cursor.h				\
	tracker-bus-fd-cursor.c				\
	tracker-bus-fd-update.c				\
	tracker-bus-fd-update.h				\
	tracker-bus-shared.c				\
	tracker-bus-shared.h

libtracker_bus_la_LDFLAGS = 	\
	-module -avoid-version -no-undefined

noinst_HEADERS = 			\
	tracker-bus.h

# Vala sources
vapi_sources =						\
	$(top_builddir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi 	\
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi 				\
	tracker-bus-fd-cursor.vapi 			\
	tracker-bus-fd-update.vapi

libtracker-bus.vala.stamp: $(libtracker_bus_la_VALASOURCES) $(vapi_sources)
	$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --includedir=libtracker-bus --pkg gio-2.0 --pkg dbus-glib-1 -H tracker-bus.h --vapi tracker-bus.vapi $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES = libtracker-bus.vala.stamp

libtracker_bus_la_LIBADD = 				\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) 					\
	$(LIBTRACKER_BUS_LIBS)

MAINTAINERCLEANFILES =					\
	$(libtracker_bus_la_VALASOURCES:.vala=.c) 	\
	libtracker-bus.vala.stamp			\
	tracker-bus.vapi				\
	tracker-bus.h

EXTRA_DIST = 						\
	$(libtracker_bus_la_VALASOURCES) \
	libtracker-bus.vala.stamp \
	tracker-bus.vapi \
	tracker-bus-fd-cursor.vapi \
	tracker-bus-fd-update.vapi


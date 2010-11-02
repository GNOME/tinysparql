include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

modules_LTLIBRARIES = libtracker-bus.la

AM_VALAFLAGS =                                         \
	-g                                             \
	--includedir=libtracker-bus                    \
	--header tracker-bus.h                         \
	--vapi tracker-bus.vapi                        \
	--pkg gio-2.0 --pkg dbus-glib-1                \
	$(top_srcdir)/src/vapi/posix.vapi              \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi     \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	tracker-bus-fd-cursor.vapi                     \
	tracker-bus-fd-update.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_BUS_CFLAGS)

libtracker_bus_la_SOURCES =                            \
	tracker-bus.vala                               \
	tracker-array-cursor.vala                      \
	tracker-bus-fd-cursor.c                        \
	tracker-bus-fd-update.c                        \
	tracker-bus-shared.c

libtracker_bus_la_LIBADD =                             \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_BUS_LIBS)

libtracker_bus_la_LDFLAGS =                            \
	-module -avoid-version -no-undefined

noinst_HEADERS =                                       \
	tracker-bus.h                                  \
	tracker-bus-fd-cursor.h                        \
	tracker-bus-fd-update.h                        \
	tracker-bus-shared.h

EXTRA_DIST =                                           \
	tracker-bus-fd-cursor.vapi                     \
	tracker-bus-fd-update.vapi


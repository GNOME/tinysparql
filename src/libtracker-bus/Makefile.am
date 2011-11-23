include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-bus.la

AM_VALAFLAGS =                                         \
	--includedir=libtracker-bus                    \
	--header tracker-bus.h                         \
	--vapi tracker-bus.vapi                        \
	--pkg gio-2.0 --pkg gio-unix-2.0 --pkg posix   \
	--vapidir $(top_srcdir)/src/vapi               \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DNO_LIBDBUS                                   \
	$(LIBTRACKER_BUS_CFLAGS)

libtracker_bus_la_SOURCES =                            \
	tracker-bus.vala                               \
	tracker-array-cursor.vala                      \
	tracker-bus-fd-cursor.vala

libtracker_bus_la_LIBADD =                             \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_BUS_LIBS)

noinst_HEADERS =                                       \
	tracker-bus.h

BUILT_SOURCES =                                        \
	libtracker_bus_la_vala.stamp

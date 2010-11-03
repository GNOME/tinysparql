include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

modules_LTLIBRARIES = libtracker-direct.la

AM_VALAFLAGS =                                         \
	-g                                             \
	--includedir=libtracker-direct                 \
	--header tracker-direct.h                      \
	--vapi tracker-direct.vapi                     \
	--pkg gio-2.0 --pkg dbus-glib-1                \
	$(top_srcdir)/src/vapi/posix.vapi              \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi     \
	$(top_srcdir)/src/libtracker-data/libtracker-data.vapi \
	$(top_srcdir)/src/libtracker-data/tracker-sparql-query.vapi \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_DIRECT_CFLAGS)

libtracker_direct_la_SOURCES =                         \
	tracker-direct.vala

libtracker_direct_la_LIBADD =                          \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_DIRECT_LIBS)

libtracker_direct_la_LDFLAGS =                         \
	-module -avoid-version -no-undefined

noinst_HEADERS =                                       \
	tracker-direct.h

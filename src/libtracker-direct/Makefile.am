include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_DIRECT_CFLAGS)

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

modules_LTLIBRARIES = libtracker-direct.la

libtracker_direct_la_VALASOURCES =                     \
	tracker-direct.vala

libtracker_direct_la_SOURCES =                         \
	libtracker-direct.vala.stamp                   \
	$(libtracker_direct_la_VALASOURCES:.vala=.c)

libtracker_direct_la_LDFLAGS =                         \
	-module -avoid-version -no-undefined

noinst_HEADERS =                                       \
	tracker-direct.h

# Vala sources
vapi_sources =                                         \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi \
	$(top_srcdir)/src/libtracker-data/libtracker-data.vapi \
	$(top_srcdir)/src/libtracker-data/tracker-sparql-query.vapi

libtracker-direct.vala.stamp: $(libtracker_direct_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --includedir=libtracker-direct --pkg gio-2.0 --pkg dbus-glib-1 -H tracker-direct.h --vapi tracker-direct.vapi $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES = libtracker-direct.vala.stamp

libtracker_direct_la_LIBADD =                          \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_DIRECT_LIBS)

MAINTAINERCLEANFILES =                                 \
	$(libtracker_direct_la_VALASOURCES:.vala=.c)   \
	libtracker-direct.vala.stamp                   \
	tracker-direct.h                               \
	tracker-direct.vapi

EXTRA_DIST =                                           \
	$(libtracker_direct_la_VALASOURCES)            \
	libtracker-direct.vala.stamp                   \
	tracker-direct.vapi

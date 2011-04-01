include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-direct.la

AM_VALAFLAGS =                                         \
	--includedir=libtracker-direct                 \
	--header tracker-direct.h                      \
	--vapi tracker-direct.vapi                     \
	--pkg gio-2.0                                  \
	$(BUILD_VALAFLAGS)                             \
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

noinst_HEADERS =                                       \
	tracker-direct.h

BUILT_SOURCES =                                        \
	libtracker_direct_la_vala.stamp

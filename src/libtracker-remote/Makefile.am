noinst_LTLIBRARIES = libtracker-remote.la

AM_VALAFLAGS =                                         \
	--includedir=libtracker-remote                 \
	--header tracker-remote.h                      \
	--vapi tracker-remote.vapi                     \
	--pkg gio-2.0                                  \
	--pkg libsoup-2.4                              \
	--pkg json-glib-1.0                            \
	--pkg libxml-2.0                               \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi

AM_CPPFLAGS =                                          \
	$(BUILD_VALACFLAGS)                            \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_REMOTE_CFLAGS)

libtracker_remote_la_SOURCES =                         \
	tracker-json-cursor.vala                       \
	tracker-xml-cursor.vala                        \
	tracker-remote.vala

libtracker_remote_la_LIBADD =                          \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_REMOTE_LIBS)

noinst_HEADERS =                                       \
	tracker-remote.h

BUILT_SOURCES =                                        \
	libtracker_remote_la_vala.stamp

DISTCLEANFILES =                                       \
	libtracker_remote_la_vala.stamp

EXTRA_DIST = meson.build

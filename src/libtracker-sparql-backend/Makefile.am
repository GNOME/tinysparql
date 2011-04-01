include $(top_srcdir)/Makefile.decl

lib_LTLIBRARIES = libtracker-sparql-@TRACKER_API_VERSION@.la

AM_VALAFLAGS =                                         \
	--pkg gio-2.0                                  \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-bus/tracker-bus.vapi \
	$(top_srcdir)/src/libtracker-direct/tracker-direct.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

libtracker_sparql_@TRACKER_API_VERSION@_la_SOURCES =   \
	tracker-backend.vala

libtracker_sparql_@TRACKER_API_VERSION@_la_LIBADD =    \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql.la \
	$(top_builddir)/src/libtracker-bus/libtracker-bus.la \
	$(top_builddir)/src/libtracker-direct/libtracker-direct.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

libtracker_sparql_@TRACKER_API_VERSION@_la_LDFLAGS =   \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
	-export-symbols-regex '^tracker_sparql_(connection|cursor|builder|escape|error|value|get)_.*'


include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-sparql.la

AM_VALAFLAGS =                                         \
	--includedir=libtracker-sparql                 \
	--header tracker-sparql.h                      \
	--vapi tracker-sparql-$(TRACKER_API_VERSION).vapi \
	--pkg gio-2.0 --pkg gmodule-2.0 --pkg posix    \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

libtracker_sparqlincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-sparql

libtracker_sparql_la_SOURCES =   \
	tracker-builder.vala                           \
	tracker-connection.vala                        \
	tracker-cursor.vala                            \
	tracker-utils.vala                             \
	tracker-uri.c

libtracker_sparql_la_LIBADD =    \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

libtracker_sparqlinclude_HEADERS =                     \
	tracker-sparql.h

tracker-sparql-$(TRACKER_API_VERSION).deps: tracker-sparql.deps
	$(AM_V_GEN)cp $^ $@

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                                            \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps

BUILT_SOURCES =                                        \
	libtracker_sparql_la_vala.stamp

MAINTAINERCLEANFILES =                                 \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.h

EXTRA_DIST =                                           \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.deps

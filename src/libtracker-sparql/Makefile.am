include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libtracker-sparql.la

AM_VALAFLAGS =                                         \
	--gir=TrackerSparql-$(TRACKER_API_VERSION).gir \
	--library TrackerSparql-$(TRACKER_API_VERSION) \
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
	tracker-uri.c                                  \
	tracker-init.c

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

# Introspection foo
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS =
INTROSPECTION_COMPILER_ARGS =

if HAVE_INTROSPECTION
INTROSPECTION_CFLAGS += $(AM_CPPFLAGS)
introspection_sources = \
	$(libtracker_sparql_@TRACKER_API_VERSION@_la_SOURCES) \
	$(libtracker_sparqlinclude_HEADERS)

TrackerSparql-0.10.gir: $(lib_LTLIBRARIES)
TrackerSparql_0_10_gir_INCLUDES = GObject-2.0 Gio-2.0 GModule-2.0
TrackerSparql_0_10_gir_CFLAGS = $(INCLUDES)
TrackerSparql_0_10_gir_LIBS = $(lib_LTLIBRARIES)
TrackerSparql_0_10_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerSparql-0.10.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(dist_gir_DATA) $(typelib_DATA)
endif

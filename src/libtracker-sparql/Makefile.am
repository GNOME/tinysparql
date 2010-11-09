include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

lib_LTLIBRARIES = libtracker-sparql-@TRACKER_API_VERSION@.la

AM_VALAFLAGS =                                         \
	-g                                             \
	--includedir=libtracker-sparql                 \
	--header tracker-sparql.h                      \
	--vapi tracker-sparql-$(TRACKER_API_VERSION).vapi \
	--pkg gio-2.0 --pkg gmodule-2.0 --pkg dbus-glib-1 \
	config.vapi                                    \
	$(top_srcdir)/src/vapi/posix.vapi              \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSPARQL_MODULES_DIR=\""$(modulesdir)"\"       \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

libtracker_sparqlincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-sparql

libtracker_sparql_@TRACKER_API_VERSION@_la_SOURCES =   \
	tracker-builder.vala                           \
	tracker-connection.vala                        \
	tracker-cursor.vala                            \
	tracker-backend.vala                           \
	tracker-utils.vala                             \
	tracker-uri.c

libtracker_sparql_@TRACKER_API_VERSION@_la_LIBADD =    \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

libtracker_sparql_@TRACKER_API_VERSION@_la_LDFLAGS =   \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

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
	libtracker_sparql_$(TRACKER_API_VERSION)_la_vala.stamp

MAINTAINERCLEANFILES =                                 \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.h

EXTRA_DIST =                                           \
	config.vapi                                    \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.deps

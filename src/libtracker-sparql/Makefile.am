include $(top_srcdir)/Makefile.decl

modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/sparql-modules

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSPARQL_MODULES_DIR=\""$(modulesdir)"\"       \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

lib_LTLIBRARIES = libtracker-sparql-@TRACKER_API_VERSION@.la

libtracker_sparqlincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-sparql

libtracker_sparql_la_VALASOURCES =                     \
	tracker-builder.vala                           \
	tracker-connection.vala                        \
	tracker-cursor.vala                            \
	tracker-backend.vala                           \
	tracker-utils.vala

libtracker_sparql_@TRACKER_API_VERSION@_la_SOURCES =   \
	libtracker-sparql.vala.stamp                   \
	$(libtracker_sparql_la_VALASOURCES:.vala=.c)   \
	tracker-uri.c

libtracker_sparql_@TRACKER_API_VERSION@_la_LDFLAGS =   \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

#	-export-symbols-regex '^tracker_.*'

libtracker_sparqlinclude_HEADERS =                     \
	tracker-sparql.h

# Vala sources
vapi_sources =                                         \
	config.vapi                                    \
	$(top_srcdir)/src/vapi/posix.vapi              \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi

libtracker-sparql.vala.stamp: $(libtracker_sparql_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --includedir=libtracker-sparql --pkg gio-2.0 --pkg gmodule-2.0 --pkg dbus-glib-1 -H tracker-sparql.h --vapi tracker-sparql-$(TRACKER_API_VERSION).vapi $^
	$(AM_V_GEN)touch $@

tracker-sparql-$(TRACKER_API_VERSION).deps: tracker-sparql.deps
	$(AM_V_GEN)cp $^ $@

BUILT_SOURCES = libtracker-sparql.vala.stamp

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                                            \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps

libtracker_sparql_@TRACKER_API_VERSION@_la_LIBADD =    \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

MAINTAINERCLEANFILES =                                 \
	$(libtracker_sparql_la_VALASOURCES:.vala=.c)   \
	libtracker-sparql.vala.stamp                   \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.h

EXTRA_DIST =                                           \
	config.vapi                                    \
	$(libtracker_sparql_la_VALASOURCES)            \
	libtracker-sparql.vala.stamp                   \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql.deps                            \
	tracker-sparql-$(TRACKER_API_VERSION).deps

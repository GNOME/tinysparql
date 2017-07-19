noinst_LTLIBRARIES = libtracker-sparql-intermediate-vala.la libtracker-sparql-intermediate-c.la

libtracker_sparql_intermediate_vala_la_VALAFLAGS =     \
	--gir Tracker_Vala-$(TRACKER_API_VERSION).gir  \
	--library tracker-sparql-$(TRACKER_API_VERSION) \
	--includedir=libtracker-sparql                 \
	--header $(vala_header:.h=-no-checks.h)        \
	--vapi tracker-sparql.vapi                     \
	--pkg gio-2.0 --pkg posix                      \
	$(BUILD_VALAFLAGS)                             \
	libtracker-sparql-intermediate-c.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_VALACFLAGS)                            \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_SPARQL_CFLAGS)

libtracker_sparqlincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-sparql

tracker-sparql-enum-types.h: tracker-sparql-enum-types.h.template tracker-notifier.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-sparql-enum-types.h.template tracker-notifier.h) > $@

tracker-sparql-enum-types.c: tracker-sparql-enum-types.c.template tracker-sparql-enum-types.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-sparql-enum-types.c.template tracker-notifier.h) > $@

ENUM_SOURCES =                                         \
	tracker-sparql-enum-types.c                    \
	tracker-sparql-enum-types.h

libtracker_sparql_intermediate_vala_la_SOURCES =       \
	tracker-namespace.vala                         \
	tracker-builder.vala                           \
	tracker-connection.vala                        \
	tracker-cursor.vala                            \
	tracker-utils.vala

libtracker_sparql_intermediate_vala_la_LIBADD =        \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)

libtracker_sparql_intermediate_c_la_SOURCES =          \
	$(ENUM_SOURCES)                                \
	tracker-namespace-manager.c                    \
	tracker-namespace-manager.h                    \
	tracker-notifier.c                             \
	tracker-notifier.h                             \
	tracker-resource.c                             \
	tracker-resource.h                             \
	tracker-uri.c                                  \
	tracker-uri.h                                  \
	tracker-ontologies.h \
	tracker-version.c \
	tracker-version.h


# Should only be one file
vala_header = tracker-generated.h

# We wrap the generated .h file with a check to make sure it's
# not included directly. All other .[ch] headers have this.
%-generated.h: %-generated-no-checks.h
	$(AM_V_GEN)(echo "#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)" ; \
	            echo "#error \"only <libtracker-sparql/tracker-sparql.h> must be included directly.\"" ; \
	            echo "#endif") > $@
	$(AM_V_GEN)(cat $< >> $@)

VAPIFILES = \
	$(srcdir)/libtracker-sparql-intermediate-c.vapi \
	$(srcdir)/tracker-sparql.vapi

tracker-sparql-$(TRACKER_API_VERSION).vapi: $(VAPIFILES)
	$(AM_V_GEN)(sed -e 's#libtracker-sparql/.*\.h#libtracker-sparql/tracker-sparql.h#' $(VAPIFILES) > $@)

libtracker_sparqlinclude_HEADERS =                     \
	$(vala_header)                                 \
	tracker-namespace-manager.h                    \
	tracker-notifier.h                             \
	tracker-ontologies.h                           \
	tracker-resource.h                             \
	tracker-sparql.h                               \
	tracker-version.h

tracker-sparql-$(TRACKER_API_VERSION).deps: tracker-sparql.deps
	$(AM_V_GEN)cp $^ $@

# PkgConfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tracker-sparql-$(TRACKER_API_VERSION).pc

tracker-sparql-$(TRACKER_API_VERSION).pc: tracker-sparql.pc
	cp $< $@

CLEANFILES = \
	$(ENUM_SOURCES)                                 \
	tracker-sparql-$(TRACKER_API_VERSION).pc

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                                            \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps

BUILT_SOURCES =                                        \
	libtracker_sparql_intermediate_vala_la_vala.stamp \
	$(ENUM_SOURCES)

MAINTAINERCLEANFILES =                                 \
	tracker-sparql-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	$(vala_header:.h=-no-checks.h)

CLEANFILES += tracker-sparql-$(TRACKER_API_VERSION).vapi

EXTRA_DIST =                                           \
	tracker-sparql-enum-types.c.template           \
	tracker-sparql-enum-types.h.template           \
	tracker-sparql-$(TRACKER_API_VERSION).deps     \
	tracker-sparql.deps                            \
	$(vala_header:.h=-no-checks.h)                 \
	tracker-sparql-add-include-guards.sh           \
	libtracker-sparql-intermediate-c.vapi          \
	meson.build

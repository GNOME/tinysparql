include $(top_srcdir)/Makefile.decl

lib_LTLIBRARIES = libtracker-sparql-@TRACKER_API_VERSION@.la

libtracker_sparqlincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-sparql

libtracker_sparql_@TRACKER_API_VERSION@_la_VALASOURCES = \
	tracker-cursor.vala

libtracker_sparql_@TRACKER_API_VERSION@_la_SOURCES = 	\
	libtracker-sparql.vala.stamp			\
	$(libtracker_sparql_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c)

libtracker_sparql_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

#	-export-symbols-regex '^tracker_.*'

libtracker_sparqlinclude_HEADERS = 			\
	tracker-sparql.h

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                     			\
	tracker-sparql-$(TRACKER_API_VERSION).vapi

# Vala sources
libtracker-sparql.vala.stamp: $(libtracker_sparql_@TRACKER_API_VERSION@_la_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-sparql.h --vapi tracker-sparql-$(TRACKER_API_VERSION).vapi $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES = 					\
	libtracker-sparql.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	$(libtracker_sparql_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c) \
	libtracker-sparql.vala.stamp			\
	tracker-sparql.h

EXTRA_DIST = 						\
	$(libtracker_sparql_@TRACKER_API_VERSION@_la_VALASOURCES) \
	libtracker-sparql.vala.stamp

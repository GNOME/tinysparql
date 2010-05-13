include $(top_srcdir)/Makefile.decl

INCLUDES = 						\
	-DLOCALEDIR=\""$(localedir)"\" 			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)/src/libtracker-client		\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(DBUS_CFLAGS)

lib_LTLIBRARIES = libtracker-client-@TRACKER_API_VERSION@.la

libtracker_clientincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-client

libtracker_client_@TRACKER_API_VERSION@_la_VALASOURCES = \
	tracker-sparql-builder.vala

libtracker_client_@TRACKER_API_VERSION@_la_SOURCES = 	\
	tracker.c					\
	tracker-client.h				\
	tracker-sparql-builder.c			\
	tracker-sparql-builder.h			\
	libtracker-client.vala.stamp			\
	$(libtracker_client_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c)

libtracker_client_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_.*'

libtracker_client_@TRACKER_API_VERSION@_la_LIBADD =	\
	$(GLIB2_LIBS) 					\
	$(DBUS_LIBS)					\
	$(GCOV_LIBS)					\
	$(GOBJECT_LIBS)

libtracker_clientinclude_HEADERS = 			\
	tracker.h					\
	tracker-client.h				\
	tracker-sparql-builder.h

# Generate DBus files from XML data.
dbus_sources = 						\
	tracker-resources-glue.h			\
	tracker-statistics-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                     			\
	tracker-client-$(TRACKER_API_VERSION).vapi

tracker-client-$(TRACKER_API_VERSION).vapi: tracker-client.vapi
	cp $< $@

# Vala sources
vapi_sources =						\
	$(top_srcdir)/src/vapi/posix.vapi		\
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi

libtracker-client.vala.stamp: $(libtracker_client_@TRACKER_API_VERSION@_la_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 -H tracker-sparql-builder.h $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES = 					\
	$(dbus_sources)					\
	libtracker-client.vala.stamp

CLEANFILES = 						\
	$(BUILT_SOURCES)				\
	$(vapi_DATA)

MAINTAINERCLEANFILES =					\
	$(libtracker_client_@TRACKER_API_VERSION@_la_VALASOURCES:.vala=.c) \
	libtracker-client.vala.stamp			\
	tracker-sparql-builder.h

EXTRA_DIST = 						\
	$(libtracker_client_@TRACKER_API_VERSION@_la_VALASOURCES) \
	tracker-client.vapi				\
	libtracker-client.vala.stamp

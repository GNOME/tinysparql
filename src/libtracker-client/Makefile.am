include $(top_srcdir)/Makefile.decl

AM_VALAFLAGS =                                         \
	-g                                             \
	--includedir=libtracker-client                 \
	--header tracker-sparql-builder.h              \
	--vapi tracker-sparql-query.vapi               \
	--pkg gio-2.0                                  \
	$(top_srcdir)/src/vapi/posix.vapi              \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi     \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_CLIENT_CFLAGS)

lib_LTLIBRARIES = libtracker-client-@TRACKER_API_VERSION@.la

libtracker_clientincludedir = $(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-client

libtracker_client_@TRACKER_API_VERSION@_la_SOURCES =   \
	tracker-sparql-builder.vala                    \
	tracker.c

libtracker_client_@TRACKER_API_VERSION@_la_LIBADD =    \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_CLIENT_LIBS)

libtracker_client_@TRACKER_API_VERSION@_la_LDFLAGS =   \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_.*'

tracker-sparql-builder.h: libtracker-client-@TRACKER_API_VERSION@.la

libtracker_clientinclude_HEADERS =                     \
	tracker.h                                      \
	tracker-client.h                               \
	tracker-sparql-builder.h

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA = tracker-client-$(TRACKER_API_VERSION).vapi

tracker-client-$(TRACKER_API_VERSION).vapi: tracker-client.vapi
	cp $< $@

# Generate DBus files from XML data.
dbus_sources =                                         \
	tracker-resources-glue.h                       \
	tracker-statistics-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES =                                        \
	libtracker_client_$(TRACKER_API_VERSION)_la_vala.stamp \
	$(dbus_sources)

CLEANFILES =                                           \
	$(dbus_sources)

MAINTAINERCLEANFILES =                                 \
	tracker-client-$(TRACKER_API_VERSION).vapi     \
	tracker-sparql-builder.h

EXTRA_DIST =                                           \
	tracker-client.vapi

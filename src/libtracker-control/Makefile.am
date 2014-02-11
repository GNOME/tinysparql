AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_MINERS_DIR=\""$(datadir)/tracker/miners"\" \
	$(LIBTRACKER_MINER_CFLAGS)

lib_LTLIBRARIES = libtracker-control-@TRACKER_API_VERSION@.la

libtracker_controldir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_controlincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-control/

libtracker_control_@TRACKER_API_VERSION@_la_SOURCES =    \
	tracker-miner-manager.c                          \
	tracker-miner-manager.h

libtracker_controlinclude_HEADERS =                      \
	tracker-control.h                                \
	tracker-miner-manager.h

libtracker_control_@TRACKER_API_VERSION@_la_LDFLAGS =    \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
if !ENABLE_GCOV 
# Using enable_gcov instead of have_unit_test because when doing a release
#  we disable gcov but NOT the unit tests
libtracker_control_@TRACKER_API_VERSION@_la_LDFLAGS +=    \
	-export-symbols-regex '^tracker_.*'
endif

libtracker_control_@TRACKER_API_VERSION@_la_LIBADD =     \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	-lm                                              \
	$(BUILD_LIBS)                                    \
	$(LIBTRACKER_MINER_LIBS)

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                                              \
	tracker-control-$(TRACKER_API_VERSION).vapi      \
	tracker-control-$(TRACKER_API_VERSION).deps

tracker-control-$(TRACKER_API_VERSION).vapi: tracker-control.vapi
	cp $< $@

tracker-control-$(TRACKER_API_VERSION).deps: tracker-control.deps
	cp $< $@

BUILT_SOURCES =                                        \
	$(vapi_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =                                           \
	tracker-control.vapi                           \
	tracker-control.deps

# Introspection foo
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	-I $(top_srcdir)/src \
	-D TRACKER_COMPILATION \
	--c-include="libtracker-control/tracker-control.h" \
	--identifier-prefix=Tracker \
	--symbol-prefix=tracker \
	--warn-all

if HAVE_INTROSPECTION
INTROSPECTION_CFLAGS += $(AM_CPPFLAGS)
introspection_sources = \
	$(libtracker_control_@TRACKER_API_VERSION@_la_SOURCES)

TrackerControl-@TRACKER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) $(lib_LTLIBRARIES) Makefile
TrackerControl_@TRACKER_API_VERSION_UNDERSCORES@_gir_INCLUDES = Gio-2.0
TrackerControl_@TRACKER_API_VERSION_UNDERSCORES@_gir_CFLAGS = $(INCLUDES)
TrackerControl_@TRACKER_API_VERSION_UNDERSCORES@_gir_NAMESPACE = "TrackerControl"
TrackerControl_@TRACKER_API_VERSION_UNDERSCORES@_gir_LIBS = $(lib_LTLIBRARIES)
TrackerControl_@TRACKER_API_VERSION_UNDERSCORES@_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerControl-$(TRACKER_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

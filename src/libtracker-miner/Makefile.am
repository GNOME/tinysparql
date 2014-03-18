# Include list of shared sources:
#  Defines:
#    $(libtracker_miner_monitor_sources)
#    $(libtracker_miner_monitor_headers)
#    $(libtracker_miner_file_system_sources)
#
# Headers and sources are split for the tests to build
# with make distcheck.
#
include Makefile-shared-sources.decl

tracker-miner-enum-types.h: tracker-miner-enum-types.h.template tracker-miner-enums.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-miner-enum-types.h.template tracker-miner-enums.h) > $@

tracker-miner-enum-types.c: tracker-miner-enum-types.c.template
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template tracker-miner-enum-types.c.template tracker-miner-enums.h) > $@

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_MINERS_DIR=\""$(datadir)/tracker/miners"\" \
	$(LIBTRACKER_MINER_CFLAGS)

lib_LTLIBRARIES = libtracker-miner-@TRACKER_API_VERSION@.la

noinst_LTLIBRARIES = libtracker-miner-private.la

libtracker_minerdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_minerincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-miner/

private_sources = 				       \
	tracker-crawler.c                              \
	tracker-crawler.h                              \
	tracker-file-notifier.h                        \
	tracker-file-notifier.c                        \
	tracker-file-system.h                          \
	tracker-file-system.c                          \
	tracker-priority-queue.h                       \
	tracker-priority-queue.c                       \
	tracker-task-pool.h                            \
	tracker-task-pool.c                            \
	tracker-sparql-buffer.h                        \
	tracker-sparql-buffer.c                        \
	tracker-thumbnailer.c                          \
	tracker-thumbnailer.h                          \
	tracker-utils.c                                \
	tracker-utils.h

if HAVE_LIBMEDIAART
private_sources += 				       \
	tracker-media-art.c                            \
	tracker-media-art.h
endif

miner_sources = 				       \
	$(libtracker_miner_monitor_sources)            \
	$(libtracker_miner_monitor_headers)            \
	tracker-decorator.c                            \
	tracker-decorator.h                            \
	tracker-decorator-internal.h                   \
	tracker-decorator-fs.c                         \
	tracker-decorator-fs.h                         \
	tracker-miner-dbus.h                           \
	tracker-miner-enum-types.c                     \
	tracker-miner-enum-types.h                     \
	tracker-miner-object.c                         \
	tracker-miner-object.h                         \
	tracker-miner-online.c                         \
	tracker-miner-online.h                         \
	tracker-miner-fs.c                             \
	tracker-miner-fs.h

libtracker_miner_private_la_SOURCES =                  \
	$(private_sources)

libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES =    \
	$(miner_sources)			       \
	tracker-indexing-tree.c                        \
	tracker-indexing-tree.h

libtracker_minerinclude_HEADERS =                      \
	tracker-decorator.h                            \
	tracker-decorator-fs.h                         \
	tracker-indexing-tree.h                        \
	tracker-miner.h                                \
	tracker-miner-common.h                         \
	tracker-miner-enums.h                          \
	tracker-miner-enum-types.h                     \
	tracker-miner-object.h                         \
	tracker-miner-online.h                         \
	tracker-miner-fs.h

libtracker_miner_@TRACKER_API_VERSION@_la_LDFLAGS =    \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
if !ENABLE_GCOV 
# Using enable_gcov instead of have_unit_test because when doing a release
#  we disable gcov but NOT the unit tests
libtracker_miner_@TRACKER_API_VERSION@_la_LDFLAGS +=    \
	-export-symbols-regex '^tracker_(miner|indexing_tree|directory_flags|filter_type|filter_policy|network_type|decorator)_.*'
endif

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD =     \
	libtracker-miner-private.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	-lm                                            \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_MINER_LIBS)

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                                            \
	tracker-miner-$(TRACKER_API_VERSION).vapi      \
	tracker-miner-$(TRACKER_API_VERSION).deps

tracker-miner-$(TRACKER_API_VERSION).vapi: tracker-miner.vapi
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).deps: tracker-miner.deps
	cp $< $@

BUILT_SOURCES =                                        \
	tracker-miner-enum-types.c                     \
	tracker-miner-enum-types.h                     \
	$(vapi_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =                                           \
	tracker-miner-enum-types.c.template            \
	tracker-miner-enum-types.h.template            \
	tracker-miner.vapi                             \
	tracker-miner.deps

# Introspection foo
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	-I $(top_srcdir)/src \
	-D TRACKER_COMPILATION \
	--c-include="libtracker-miner/tracker-miner.h" \
	--identifier-prefix=Tracker \
	--symbol-prefix=tracker \
	--warn-all

INTROSPECTION_COMPILER_ARGS = --includedir=$(top_srcdir)/src/libtracker-sparql

if HAVE_INTROSPECTION
INTROSPECTION_CFLAGS += $(AM_CPPFLAGS)
introspection_sources = \
	$(filter-out $(libtracker_miner_monitor_sources) \
	             $(libtracker_miner_monitor_headers)  \
		     $(private_sources),  \
	  $(libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES))

TrackerMiner-@TRACKER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) $(lib_LTLIBRARIES) Makefile
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_INCLUDES = GObject-2.0 GModule-2.0 Gio-2.0
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_srcdir)/src/libtracker-sparql/Tracker-$(TRACKER_API_VERSION).gir
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_CFLAGS = $(INCLUDES)
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_NAMESPACE = "TrackerMiner"
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_LIBS = $(lib_LTLIBRARIES)
TrackerMiner_@TRACKER_API_VERSION_UNDERSCORES@_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerMiner-$(TRACKER_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

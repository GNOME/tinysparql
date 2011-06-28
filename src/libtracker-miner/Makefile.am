include $(top_srcdir)/Makefile.decl

# Include list of shared sources:
#  Defines:
#    $(libtracker_miner_marshal_sources)
#    $(libtracker_miner_marshal_headers)
#    $(libtracker_miner_monitor_sources)
#    $(libtracker_miner_monitor_headers)
#
# Headers and sources are split for the tests to build
# with make distcheck.
#
include Makefile-shared-sources.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_MINERS_DIR=\""$(datadir)/tracker/miners"\" \
	$(LIBTRACKER_MINER_CFLAGS)

lib_LTLIBRARIES = libtracker-miner-@TRACKER_API_VERSION@.la

libtracker_minerdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_minerincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-miner/

crawler_sources = 				       \
	$(libtracker_miner_marshal_sources)            \
	$(libtracker_miner_marshal_headers)            \
	tracker-crawler.c                              \
	tracker-crawler.h                              \
	tracker-utils.c                                \
	tracker-utils.h					

private_sources = 				       \
	tracker-priority-queue.h                       \
	tracker-priority-queue.c                       \
	tracker-miner-fs-processing-pool.h             \
	tracker-miner-fs-processing-pool.c             \
	tracker-storage.c                              \
	tracker-storage.h                              

miner_sources = 				       \
	$(libtracker_miner_monitor_sources)            \
	$(libtracker_miner_monitor_headers)            \
	tracker-miner-dbus.h                           \
	tracker-miner-object.c                         \
	tracker-miner-object.h                         \
	tracker-miner-fs.c                             \
	tracker-miner-fs.h                             \
	tracker-miner-manager.c                        \
	tracker-miner-manager.h                        \
	tracker-miner-web.c                            \
	tracker-miner-web.h                            


libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES =    \
	$(network_provider_sources)                    \
	$(password_provider_sources)                   \
	$(crawler_sources)			       \
	$(miner_sources)			       \
	$(private_sources)			       \
	tracker-network-provider.c                     \
	tracker-password-provider.c                    \
	tracker-thumbnailer.c                          \
	tracker-thumbnailer.h                          \
	tracker-albumart.c                             \
	tracker-albumart.h

libtracker_minerinclude_HEADERS =                      \
	tracker-albumart.h                             \
	tracker-crawler.h                              \
	tracker-miner.h                                \
	tracker-miner-common.h                         \
	tracker-miner-object.h                         \
	tracker-miner-fs.h                             \
	tracker-miner-manager.h                        \
	tracker-miner-web.h                            \
	tracker-network-provider.h                     \
	tracker-password-provider.h                    \
	tracker-storage.h                              \
	tracker-thumbnailer.h

libtracker_miner_@TRACKER_API_VERSION@_la_LDFLAGS =    \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_(miner|thumbnailer|crawler|storage|password_provider|network_provider)_.*'

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD =     \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	-lm                                            \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_MINER_LIBS)

if HAVE_GNOME_KEYRING
password_provider_sources = tracker-password-provider-gnome.c
else
password_provider_sources = tracker-password-provider-keyfile.c
endif

if HAVE_NETWORK_MANAGER
network_provider_sources = tracker-network-provider-network-manager.c
else
network_provider_sources = tracker-network-provider-dummy.c
endif

$(top_builddir)/src/libtracker-miner/tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

$(top_builddir)/src/libtracker-miner/tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)(echo "#include \"tracker-marshal.h\""; \
		    $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body) > $@

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
	$(libtracker_miner_marshal_sources)            \
	$(libtracker_miner_marshal_headers)            \
	$(vapi_DATA)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =                                           \
	tracker-marshal.list                           \
	tracker-miner.vapi                             \
	tracker-miner.deps

# Introspection foo
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	-I $(top_srcdir)/src \
	-D TRACKER_COMPILATION \
	--c-include="libtracker-miner/tracker-miner.h" \
	--identifier-prefix=TrackerProcessor \
	--symbol-prefix=tracker_processing \
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
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_INCLUDES = GObject-2.0 GModule-2.0 Gio-2.0
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_srcdir)/src/libtracker-sparql/Tracker-$(TRACKER_API_VERSION).gir
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_CFLAGS = $(INCLUDES)
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_NAMESPACE = "TrackerMiner"
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_LIBS = $(lib_LTLIBRARIES)
TrackerMiner_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerMiner-$(TRACKER_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

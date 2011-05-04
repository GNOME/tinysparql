include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_EXTRACTOR_RULES_DIR=\""$(datadir)/tracker/extract-rules"\" \
	$(LIBTRACKER_EXTRACT_CFLAGS)

lib_LTLIBRARIES = libtracker-extract-@TRACKER_API_VERSION@.la

libtracker_extractincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-extract/

libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES =  \
	tracker-data.h                                 \
	tracker-encoding.c                             \
	tracker-exif.c                                 \
	tracker-exif.h                                 \
	tracker-extract-client.c                       \
	tracker-extract-client.h                       \
	tracker-guarantee.c                            \
	tracker-guarantee.h                            \
	tracker-iptc.c                                 \
	tracker-iptc.h                                 \
	tracker-module-manager.c                       \
	tracker-module-manager.h                       \
	tracker-utils.c                                \
	tracker-xmp.c                                  \
	tracker-xmp.h

noinst_HEADERS =

libtracker_extractinclude_HEADERS =                    \
	tracker-data.h                                 \
	tracker-encoding.h                             \
	tracker-exif.h                                 \
	tracker-extract-client.h                       \
	tracker-extract.h                              \
	tracker-guarantee.h                            \
	tracker-iptc.h                                 \
	tracker-module-manager.h                       \
	tracker-utils.h                                \
	tracker-xmp.h

if HAVE_ENCA
libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES += \
	tracker-encoding-enca.c                        \
	tracker-encoding-enca.h
endif

if HAVE_MEEGOTOUCH
libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES += \
	tracker-encoding-meegotouch.cpp                \
	tracker-encoding-meegotouch.h
endif

libtracker_extract_@TRACKER_API_VERSION@_la_LDFLAGS =  \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_.*'

libtracker_extract_@TRACKER_API_VERSION@_la_LIBADD =   \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_EXTRACT_LIBS)

# Introspection foo
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--warn-all \
	-I $(top_srcdir)/src \
	-D TRACKER_COMPILATION \
	--c-include="libtracker-extract/tracker-extract.h"

INTROSPECTION_COMPILER_ARGS = --includedir=$(top_srcdir)/src/libtracker-sparql

if HAVE_INTROSPECTION
INTROSPECTION_CFLAGS += $(AM_CPPFLAGS)
introspection_sources = $(libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES)
TrackerExtract-@TRACKER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) $(lib_LTLIBRARIES) Makefile
TrackerExtract_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_INCLUDES = GObject-2.0 GModule-2.0
TrackerExtract_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_srcdir)/src/libtracker-sparql/Tracker-$(TRACKER_API_VERSION).gir
TrackerExtract_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_CFLAGS = $(INCLUDES)
TrackerExtract_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_LIBS = $(lib_LTLIBRARIES)
TrackerExtract_@TRACKER_MAJOR_VERSION@_@TRACKER_MINOR_VERSION@_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerExtract-$(TRACKER_API_VERSION).gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

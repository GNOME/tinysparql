include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	$(LIBTRACKER_EXTRACT_CFLAGS)

lib_LTLIBRARIES = libtracker-extract-@TRACKER_API_VERSION@.la

libtracker_extractincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-extract/

libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES =  \
	tracker-data.h                                 \
	tracker-exif.c                                 \
	tracker-exif.h                                 \
	tracker-guarantee.c                            \
	tracker-guarantee.h                            \
	tracker-iptc.c                                 \
	tracker-iptc.h                                 \
	tracker-utils.c                                \
	tracker-xmp.c                                  \
	tracker-xmp.h

noinst_HEADERS =

libtracker_extractinclude_HEADERS =                    \
	tracker-data.h                                 \
	tracker-exif.h                                 \
	tracker-extract.h                              \
	tracker-guarantee.h                            \
	tracker-iptc.h                                 \
	tracker-utils.h                                \
	tracker-xmp.h

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
	-I $(top_builddir)/src \
	-D TRACKER_COMPILATION \
	--c-include="libtracker-extract/tracker-extract.h" \
	--add-include-path=$(top_builddir)/src/libtracker-sparql 
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/src/libtracker-sparql

if HAVE_INTROSPECTION
INTROSPECTION_CFLAGS += $(AM_CPPFLAGS)
introspection_sources = $(libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES)

TrackerExtract-0.10.gir: $(INTROSPECTION_SCANNER) $(lib_LTLIBRARIES) Makefile
TrackerExtract_0_10_gir_INCLUDES = GObject-2.0 GModule-2.0 Tracker-0.10
TrackerExtract_0_10_gir_CFLAGS = $(INCLUDES)
TrackerExtract_0_10_gir_LIBS = $(lib_LTLIBRARIES)
TrackerExtract_0_10_gir_FILES = $(introspection_sources)

INTROSPECTION_GIRS += TrackerExtract-0.10.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(dist_gir_DATA) $(typelib_DATA)
endif

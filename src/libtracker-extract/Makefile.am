include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_EXTRACTOR_RULES_DIR=\""$(datadir)/tracker/extract-rules"\" \
	$(LIBTRACKER_EXTRACT_CFLAGS)

lib_LTLIBRARIES = libtracker-extract-@TRACKER_API_VERSION@.la

libtracker_extractincdir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-extract/

libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES =  \
	tracker-data.h                                 \
	tracker-exif.c                                 \
	tracker-exif.h                                 \
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

libtracker_extractinc_HEADERS =                        \
	tracker-data.h                                 \
	tracker-exif.h                                 \
	tracker-extract.h                              \
	tracker-guarantee.h                            \
	tracker-iptc.h                                 \
	tracker-module-manager.h                       \
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

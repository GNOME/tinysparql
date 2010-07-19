include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)/src				\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(LIBIPTCDATA_CFLAGS)				\
	$(LIBEXIF_CFLAGS) 				\
	$(EXEMPI_CFLAGS)

lib_LTLIBRARIES = libtracker-extract-@TRACKER_API_VERSION@.la

libtracker_extractincdir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-extract/

libtracker_extract_@TRACKER_API_VERSION@_la_SOURCES =	\
	tracker-data.h					\
	tracker-exif.c					\
	tracker-exif.h					\
	tracker-iptc.c					\
	tracker-iptc.h					\
	tracker-utils.c					\
	tracker-xmp.c					\
	tracker-xmp.h

noinst_HEADERS =

libtracker_extractinc_HEADERS =				\
	tracker-data.h					\
	tracker-exif.h					\
	tracker-extract.h				\
	tracker-iptc.h					\
	tracker-utils.h					\
	tracker-xmp.h

libtracker_extract_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_.*'

libtracker_extract_@TRACKER_API_VERSION@_la_LIBADD = 	\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(LIBIPTCDATA_LIBS)				\
	$(LIBEXIF_LIBS)					\
	$(EXEMPI_LIBS)

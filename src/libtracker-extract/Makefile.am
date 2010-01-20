include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-I$(top_srcdir)/src				\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	$(GIO_CFLAGS)					\
	$(LIBIPTCDATA_CFLAGS)				\
	$(LIBEXIF_CFLAGS) 				\
	$(EXEMPI_CFLAGS)

libtracker_extractdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_extractincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-extract/
libtracker_extract_LTLIBRARIES = libtracker-extract.la

# $(marshal_sources)				

libtracker_extract_la_SOURCES =	 			\
	tracker-extract.h				\
	tracker-exif.c					\
	tracker-exif.h					\
	tracker-iptc.c					\
	tracker-iptc.h					\
	tracker-xmp.c					\
	tracker-xmp.h

noinst_HEADERS =

libtracker_extractinclude_HEADERS =			\
	tracker-extract.h				\
	tracker-exif.h					\
	tracker-iptc.h					\
	tracker-xmp.h

libtracker_extract_la_LDFLAGS = 			\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_extract_la_LIBADD = 				\
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(GDKPIXBUF_LIBS)				\
	$(LIBIPTCDATA_LIBS)				\
	$(LIBEXIF_LIBS)					\
	$(EXEMPI_LIBS)

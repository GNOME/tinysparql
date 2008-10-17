include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker/extract-modules

INCLUDES = 								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMODULESDIR=\"$(modulesdir)\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src 						\
	$(EXEMPI_CFLAGS) 						\
	$(LIBJPEG_CFLAGS) 						\
	$(LIBTIFF_CFLAGS) 						\
	$(LIBEXIF_CFLAGS) 						\
	$(LIBGSF_CFLAGS) 						\
	$(LIBXML2_CFLAGS) 						\
	$(LIBPNG_CFLAGS) 						\
	$(POPPLER_GLIB_CFLAGS) 						\
	$(GSTREAMER_CFLAGS) 						\
	$(XINE_CFLAGS) 							\
	$(GDKPIXBUF_CFLAGS)						\
	$(GMODULE_CFLAGS) 						\
	$(DBUS_CFLAGS)							\
	$(GLIB2_CFLAGS)

modules_LTLIBRARIES = 							\
	libextract-abw.la 						\
	libextract-m3u.la						\
	libextract-imagemagick.la 					\
	libextract-mp3.la				 		\
	libextract-mplayer.la 						\
	libextract-oasis.la 						\
	libextract-png.la 						\
	libextract-ps.la 						\
	libextract-totem.la

if HAVE_EXEMPI
modules_LTLIBRARIES += libextract-xmp.la
endif

if HAVE_LIBJPEG
modules_LTLIBRARIES += libextract-jpeg.la
endif

if HAVE_LIBTIFF
modules_LTLIBRARIES += libextract-tiff.la
endif

if HAVE_LIBXML2
modules_LTLIBRARIES += libextract-html.la
endif

if HAVE_LIBGSF
modules_LTLIBRARIES += libextract-msoffice.la
endif

if HAVE_POPPLER_GLIB
modules_LTLIBRARIES += libextract-pdf.la
endif

if HAVE_GSTREAMER
modules_LTLIBRARIES += libextract-gstreamer.la
endif

if HAVE_LIBXINE
modules_LTLIBRARIES += libextract-xine.la
endif


# Common XMP Sources
xmp_sources = 								\
	tracker-xmp.c							\
	tracker-xmp.h

# Common AlbumArt sources

albumart_libs = 
albumart_flags = 
albumart_sources =							\
	tracker-albumart.c						\
	tracker-albumart.h

if HAVE_GDKPIXBUF
albumart_flags += $(DBUS_CFLAGS)
albumart_libs += 							\
	$(GDKPIXBUF_LIBS) $(DBUS_LIBS)
endif

# ABW
libextract_abw_la_SOURCES = tracker-extract-abw.c
libextract_abw_la_LDFLAGS = $(module_flags)
libextract_abw_la_LIBADD = $(GLIB2_LIBS)

# Imagemagick
libextract_imagemagick_la_SOURCES = tracker-extract-imagemagick.c $(xmp_sources)
libextract_imagemagick_la_LDFLAGS = $(module_flags)
libextract_imagemagick_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

# M3U
libextract_m3u_la_SOURCES = tracker-extract-m3u.c
libextract_m3u_la_LDFLAGS = $(module_flags)
libextract_m3u_la_LIBADD = $(GLIB2_LIBS)

# MP3
libextract_mp3_la_SOURCES = tracker-extract-mp3.c $(albumart_sources)
libextract_mp3_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_mp3_la_LIBADD = $(albumart_libs) $(GLIB2_LIBS)

# MPlayer
libextract_mplayer_la_SOURCES = tracker-extract-mplayer.c
libextract_mplayer_la_LDFLAGS = $(module_flags)
libextract_mplayer_la_LIBADD = $(GLIB2_LIBS)

# Oasis
libextract_oasis_la_SOURCES = tracker-extract-oasis.c
libextract_oasis_la_LDFLAGS = $(module_flags)
libextract_oasis_la_LIBADD = $(GLIB2_LIBS)

# PNG
libextract_png_la_SOURCES = tracker-extract-png.c $(xmp_sources)
libextract_png_la_LDFLAGS = $(module_flags)
libextract_png_la_LIBADD = $(GLIB2_LIBS) $(LIBPNG_LIBS) $(EXEMPI_LIBS)

# PS
libextract_ps_la_SOURCES = tracker-extract-ps.c
libextract_ps_la_LDFLAGS = $(module_flags)
libextract_ps_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS)

# Totem
libextract_totem_la_SOURCES = tracker-extract-totem.c
libextract_totem_la_LDFLAGS = $(module_flags)
libextract_totem_la_LIBADD = $(GLIB2_LIBS)

# XMP
libextract_xmp_la_SOURCES = tracker-extract-xmp.c $(xmp_sources)
libextract_xmp_la_LDFLAGS = $(module_flags)
libextract_xmp_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

# HTML
libextract_html_la_SOURCES = tracker-extract-html.c
libextract_html_la_LDFLAGS = $(module_flags)
libextract_html_la_LIBADD = $(GLIB2_LIBS) $(LIBXML2_LIBS)

# MS Office
libextract_msoffice_la_SOURCES = tracker-extract-msoffice.c
libextract_msoffice_la_LDFLAGS = $(module_flags)
libextract_msoffice_la_LIBADD = $(GLIB2_LIBS) $(LIBGSF_LIBS)

# PDF
libextract_pdf_la_SOURCES = tracker-extract-pdf.c $(xmp_sources)
libextract_pdf_la_LDFLAGS = $(module_flags)
libextract_pdf_la_LIBADD = $(GLIB2_LIBS) $(POPPLER_GLIB_LIBS) $(EXEMPI_LIBS)

# GStreamer
libextract_gstreamer_la_SOURCES = tracker-extract-gstreamer.c $(albumart_sources)
libextract_gstreamer_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_gstreamer_la_LIBADD = $(albumart_libs) $(GSTREAMER_LIBS) $(GLIB2_LIBS)

# Xine
libextract_xine_la_SOURCES = tracker-extract-libxine.c
libextract_xine_la_LDFLAGS = $(module_flags)
libextract_xine_la_LIBADD = $(GLIB2_LIBS) $(XINE_LIBS)

# JPEG
libextract_jpeg_la_SOURCES = tracker-extract-jpeg.c $(xmp_sources)
libextract_jpeg_la_LDFLAGS = $(module_flags)
libextract_jpeg_la_LIBADD = $(GLIB2_LIBS) $(LIBJPEG_LIBS) $(LIBEXIF_LIBS) $(EXEMPI_LIBS)

# TIFF
libextract_tiff_la_SOURCES = tracker-extract-tiff.c $(xmp_sources)
libextract_tiff_la_LDFLAGS = $(module_flags)
libextract_tiff_la_LIBADD = $(GLIB2_LIBS) $(LIBTIFF_LIBS) $(EXEMPI_LIBS)

#
# Binaries
#
libexec_PROGRAMS = tracker-extract

tracker_extract_SOURCES = 						\
	tracker-extract.c						\
	tracker-extract.h

tracker_extract_LDADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

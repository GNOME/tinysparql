include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker/extract-modules

INCLUDES = 								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMODULESDIR=\"$(modulesdir)\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
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
	$(GLIB2_CFLAGS)							\
	$(TOTEM_PL_PARSER_CFLAGS)

modules_LTLIBRARIES = 							\
	libextract-abw.la 						\
	libextract-mp3.la				 		\
	libextract-oasis.la 						\
	libextract-png.la 						\
	libextract-ps.la 						


# if HAVE_IMAGEMAGICK
# modules_LTLIBRARIES += libextract-imagemagick.la
# endif

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

#if HAVE_GSTREAMER
#modules_LTLIBRARIES += libextract-gstreamer-tagreadbin.la
#endif

if HAVE_GST_HELIX
modules_LTLIBRARIES += libextract-gstreamer-helix.la
endif 

if HAVE_LIBXINE
modules_LTLIBRARIES += libextract-xine.la
endif

if USING_EXTERNAL_VIDEO_PLAYER
modules_LTLIBRARIES +=	libextract-mplayer.la 				\
			libextract-totem.la
endif

if HAVE_TOTEM_PL_PARSER
modules_LTLIBRARIES += libextract-playlist.la
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

escape_sources = 							\
	tracker-escape.c						\
	tracker-escape.h

# ABW
libextract_abw_la_SOURCES = tracker-extract-abw.c $(escape_sources)
libextract_abw_la_LDFLAGS = $(module_flags)
libextract_abw_la_LIBADD = $(GLIB2_LIBS)

# Imagemagick
# libextract_imagemagick_la_SOURCES = tracker-extract-imagemagick.c $(xmp_sources)
# libextract_imagemagick_la_LDFLAGS = $(module_flags)
# libextract_imagemagick_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

# MP3
libextract_mp3_la_SOURCES = tracker-extract-mp3.c $(albumart_sources) $(escape_sources)
libextract_mp3_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_mp3_la_LIBADD = $(albumart_libs) $(GLIB2_LIBS)

# MPlayer
libextract_mplayer_la_SOURCES = tracker-extract-mplayer.c $(escape_sources)
libextract_mplayer_la_LDFLAGS = $(module_flags)
libextract_mplayer_la_LIBADD = $(GLIB2_LIBS)

# Oasis
libextract_oasis_la_SOURCES = tracker-extract-oasis.c $(escape_sources)
libextract_oasis_la_LDFLAGS = $(module_flags)
libextract_oasis_la_LIBADD = $(GLIB2_LIBS)

# PNG
libextract_png_la_SOURCES = tracker-extract-png.c $(xmp_sources) $(escape_sources)
libextract_png_la_LDFLAGS = $(module_flags)
libextract_png_la_LIBADD = $(GLIB2_LIBS) $(LIBPNG_LIBS) $(EXEMPI_LIBS)

# PS
libextract_ps_la_SOURCES = tracker-extract-ps.c $(escape_sources)
libextract_ps_la_LDFLAGS = $(module_flags)
libextract_ps_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS)

# Totem
libextract_totem_la_SOURCES = tracker-extract-totem.c $(escape_sources)
libextract_totem_la_LDFLAGS = $(module_flags)
libextract_totem_la_LIBADD = $(GLIB2_LIBS)

# XMP
libextract_xmp_la_SOURCES = tracker-extract-xmp.c $(xmp_sources) $(escape_sources)
libextract_xmp_la_LDFLAGS = $(module_flags)
libextract_xmp_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

# HTML
libextract_html_la_SOURCES = tracker-extract-html.c $(escape_sources)
libextract_html_la_LDFLAGS = $(module_flags)
libextract_html_la_LIBADD = $(GLIB2_LIBS) $(LIBXML2_LIBS)

# MS Office
libextract_msoffice_la_SOURCES = tracker-extract-msoffice.c $(escape_sources)
libextract_msoffice_la_LDFLAGS = $(module_flags)
libextract_msoffice_la_LIBADD = $(GLIB2_LIBS) $(LIBGSF_LIBS)

# PDF
libextract_pdf_la_SOURCES = tracker-extract-pdf.c $(xmp_sources) $(escape_sources)
libextract_pdf_la_LDFLAGS = $(module_flags)
libextract_pdf_la_LIBADD = $(GLIB2_LIBS) $(POPPLER_GLIB_LIBS) $(EXEMPI_LIBS)

# GStreamer
libextract_gstreamer_la_SOURCES = tracker-extract-gstreamer.c $(albumart_sources) $(escape_sources)
libextract_gstreamer_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_gstreamer_la_LIBADD = $(albumart_libs) $(GSTREAMER_LIBS) $(GLIB2_LIBS)

# GStreamer tagreadbin
#libextract_gstreamer_tagreadbin_la_SOURCES = tracker-extract-gstreamer-tagreadbin.c $(albumart_sources) $(escape_sources)
#libextract_gstreamer_tagreadbin_la_LDFLAGS = $(module_flags) $(albumart_flags)
#libextract_gstreamer_tagreadbin_la_LIBADD = $(albumart_libs) $(GSTREAMER_LIBS) $(GLIB2_LIBS)

# GStreamer helix
libextract_gstreamer_helix_la_SOURCES = tracker-extract-gstreamer-helix.c $(albumart_sources) $(escape_sources)
libextract_gstreamer_helix_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_gstreamer_helix_la_LIBADD = $(albumart_libs) $(GSTREAMER_LIBS) $(GLIB2_LIBS)

# Xine
libextract_xine_la_SOURCES = tracker-extract-libxine.c $(escape_sources)
libextract_xine_la_LDFLAGS = $(module_flags)
libextract_xine_la_LIBADD = $(GLIB2_LIBS) $(XINE_LIBS)

# JPEG
libextract_jpeg_la_SOURCES = tracker-extract-jpeg.c $(xmp_sources) $(escape_sources)
libextract_jpeg_la_LDFLAGS = $(module_flags)
libextract_jpeg_la_LIBADD = $(GLIB2_LIBS) $(LIBJPEG_LIBS) $(LIBEXIF_LIBS) $(EXEMPI_LIBS)

# TIFF
libextract_tiff_la_SOURCES = tracker-extract-tiff.c $(xmp_sources) $(escape_sources)
libextract_tiff_la_LDFLAGS = $(module_flags)
libextract_tiff_la_LIBADD = $(GLIB2_LIBS) $(LIBTIFF_LIBS) $(EXEMPI_LIBS)

# Playlists using totem-pl-parser
libextract_playlist_la_SOURCES = tracker-extract-playlist.c  $(escape_sources)
libextract_playlist_la_LDFLAGS = $(module_flags)
libextract_playlist_la_LIBADD = $(GLIB2_LIBS) $(TOTEM_PL_PARSER_LIBS)

#
# Binaries
#
libexec_PROGRAMS = tracker-extract

tracker_extract_SOURCES = 						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-extract.c						\
	tracker-extract.h						\
	tracker-main.c							\
	tracker-main.h

tracker_extract_LDADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)

dbus_sources = 								\
	tracker-extract-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
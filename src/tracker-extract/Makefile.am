include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/extract-modules

INCLUDES = 								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DTRACKER_EXTRACTORS_DIR=\"$(modulesdir)\"			\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src 						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GDKPIXBUF_CFLAGS)						\
	$(GMODULE_CFLAGS) 						\
	$(DBUS_CFLAGS)							\
	$(LIBGIF_CFLAGS)						\
	$(LIBJPEG_CFLAGS) 						\
	$(LIBTIFF_CFLAGS) 						\
	$(LIBGSF_CFLAGS) 						\
	$(LIBXML2_CFLAGS) 						\
	$(LIBPNG_CFLAGS) 						\
	$(POPPLER_CFLAGS) 						\
	$(GSTREAMER_CFLAGS) 						\
	$(XINE_CFLAGS) 							\
	$(TOTEM_PL_PARSER_CFLAGS)

if HAVE_LIBSTREAMANALYZER
INCLUDES += $(LIBSTREAMANALYZER_CFLAGS)
endif

if HAVE_ENCA
INCLUDES += $(ENCA_CFLAGS)
endif

# NOTE:
# We don't always link with libtracker-common, we only link
# against it if we directly use functions in the .so
modules_LTLIBRARIES = 							\
	libextract-abw.la 						\
	libextract-mp3.la				 		\
	libextract-png.la 						\
	libextract-ps.la 						\
	libextract-text.la

if HAVE_LIBVORBIS
modules_LTLIBRARIES += libextract-vorbis.la
endif

if HAVE_LIBFLAC
modules_LTLIBRARIES += libextract-flac.la
endif

if HAVE_EXEMPI
modules_LTLIBRARIES += libextract-xmp.la
endif

if HAVE_LIBGIF
modules_LTLIBRARIES += libextract-gif.la
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
modules_LTLIBRARIES += 							\
	libextract-msoffice.la						\
	libextract-oasis.la
endif

if HAVE_POPPLER
modules_LTLIBRARIES += libextract-pdf.la
endif

if HAVE_GSTREAMER
modules_LTLIBRARIES += libextract-gstreamer.la
endif

if HAVE_GSTREAMER_HELIX
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

# ABW
libextract_abw_la_SOURCES = tracker-extract-abw.c
libextract_abw_la_LDFLAGS = $(module_flags)
libextract_abw_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# MP3
libextract_mp3_la_SOURCES = tracker-extract-mp3.c
libextract_mp3_la_LDFLAGS = $(module_flags)
libextract_mp3_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

if HAVE_ENCA
libextract_mp3_la_LIBADD += $(ENCA_LIBS)
#libextract_mp3_la_LIBADD += -L/usr/lib/libenca.a
endif

# Vorbis (OGG)
libextract_vorbis_la_SOURCES = tracker-extract-vorbis.c $(escape_sources)
libextract_vorbis_la_LDFLAGS = $(module_flags)
libextract_vorbis_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBVORBIS_LIBS)						\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

# Flac
libextract_flac_la_SOURCES = tracker-extract-flac.c $(escape_sources)
libextract_flac_la_LDFLAGS = $(module_flags)
libextract_flac_la_LIBADD =						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBFLAC_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

# MPlayer
libextract_mplayer_la_SOURCES = tracker-extract-mplayer.c
libextract_mplayer_la_LDFLAGS = $(module_flags)
libextract_mplayer_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GLIB2_LIBS)

# Oasis
libextract_oasis_la_SOURCES = tracker-extract-oasis.c
libextract_oasis_la_LDFLAGS = $(module_flags)
libextract_oasis_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# PNG
libextract_png_la_SOURCES = tracker-extract-png.c
libextract_png_la_LDFLAGS = $(module_flags)
libextract_png_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS) 							\
	$(LIBPNG_LIBS) 							\
	$(GCOV_LIBS)

# PS
libextract_ps_la_SOURCES = tracker-extract-ps.c
libextract_ps_la_LDFLAGS = $(module_flags)
libextract_ps_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS)                                                   \
	$(GCOV_LIBS)

# Totem
libextract_totem_la_SOURCES = tracker-extract-totem.c
libextract_totem_la_LDFLAGS = $(module_flags)
libextract_totem_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# XMP
libextract_xmp_la_SOURCES = tracker-extract-xmp.c
libextract_xmp_la_LDFLAGS = $(module_flags)
libextract_xmp_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# HTML
libextract_html_la_SOURCES = tracker-extract-html.c
libextract_html_la_LDFLAGS = $(module_flags)
libextract_html_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GLIB2_LIBS) 							\
	$(LIBXML2_LIBS) 						\
	$(GCOV_LIBS)

# MS Office
libextract_msoffice_la_SOURCES = tracker-extract-msoffice.c
libextract_msoffice_la_LDFLAGS = $(module_flags)
libextract_msoffice_la_LIBADD = 					\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBGSF_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

# PDF
libextract_pdf_la_SOURCES = tracker-extract-pdf.cpp
libextract_pdf_la_LDFLAGS = $(module_flags)
libextract_pdf_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(POPPLER_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

# GStreamer
libextract_gstreamer_la_SOURCES = tracker-extract-gstreamer.c
libextract_gstreamer_la_LDFLAGS = $(module_flags)
libextract_gstreamer_la_LIBADD = 					\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GSTREAMER_LIBS)						\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

# GStreamer helix
libextract_gstreamer_helix_la_SOURCES = tracker-extract-gstreamer-helix.c
libextract_gstreamer_helix_la_LDFLAGS = $(module_flags) $(albumart_flags)
libextract_gstreamer_helix_la_LIBADD = 					\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GSTREAMER_LIBS) 						\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# Xine
libextract_xine_la_SOURCES = tracker-extract-libxine.c
libextract_xine_la_LDFLAGS = $(module_flags)
libextract_xine_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(XINE_LIBS)							\
	$(GLIB2_LIBS)  							\
	$(GCOV_LIBS)

# GIF
libextract_gif_la_SOURCES = tracker-extract-gif.c
libextract_gif_la_LDFLAGS = $(module_flags)
libextract_gif_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBGIF_LIBS)	 						\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# JPEG
libextract_jpeg_la_SOURCES = tracker-extract-jpeg.c
libextract_jpeg_la_LDFLAGS = $(module_flags)
libextract_jpeg_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(LIBJPEG_LIBS) 						\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# TIFF
libextract_tiff_la_SOURCES = tracker-extract-tiff.c $(xmp_sources) $(iptc_sources)
libextract_tiff_la_LDFLAGS = $(module_flags)
libextract_tiff_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(LIBTIFF_LIBS) 						\
	$(GLIB2_LIBS) 							\
	$(GCOV_LIBS)

# Playlists using totem-pl-parser
libextract_playlist_la_SOURCES = tracker-extract-playlist.c
libextract_playlist_la_LDFLAGS = $(module_flags)
libextract_playlist_la_LIBADD = 					\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(TOTEM_PL_PARSER_LIBS) 					\
	$(GLIB2_LIBS)  							\
	$(GCOV_LIBS)

# TIFF
libextract_text_la_SOURCES = tracker-extract-text.c
libextract_text_la_LDFLAGS = $(module_flags)
libextract_text_la_LIBADD = 						\
	$(top_builddir)/src/libtracker-extract/libtracker-extract-@TRACKER_API_VERSION@.la \
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)  							\
	$(GCOV_LIBS)

#
# Binaries
#
libexec_PROGRAMS = tracker-extract

tracker_extract_SOURCES = 						\
	$(marshal_sources)						\
	$(dbus_sources)							\
	tracker-albumart.c						\
	tracker-albumart.h						\
	tracker-config.c						\
	tracker-config.h						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-extract.c						\
	tracker-extract.h						\
	tracker-fts-config.c						\
	tracker-fts-config.h						\
	tracker-main.c							\
	tracker-main.h							\
	tracker-albumart-generic.h

tracker_extract_LDADD = 						\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)

if HAVE_LIBGSF
tracker_extract_SOURCES += tracker-gsf.c tracker-gsf.h
tracker_extract_LDADD += $(LIBGSF_LIBS)
endif

if HAVE_LIBSTREAMANALYZER
tracker_extract_SOURCES += tracker-topanalyzer.cpp tracker-topanalyzer.h
tracker_extract_LDADD += $(LIBSTREAMANALYZER_LIBS)
endif

if HAVE_GDKPIXBUF
tracker_extract_LDADD += $(GDKPIXBUF_LIBS)
tracker_extract_SOURCES += tracker-albumart-pixbuf.c
else
if HAVE_QUILL
INCLUDES += $(QUILL_CFLAGS)
tracker_extract_SOURCES += tracker-albumart-quill.cpp
tracker_extract_LDADD += $(QUILL_LIBS)
else
tracker_extract_SOURCES += tracker-albumart-dummy.c
endif
endif

marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

dbus_sources = 								\
	tracker-extract-glue.h

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ &&	\
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(marshal_sources)						\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

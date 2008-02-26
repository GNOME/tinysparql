module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker/extract-modules

INCLUDES = \
	$(GLIB2_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(EXEMPI_CFLAGS) \
	$(LIBEXIF_CFLAGS) \
	$(LIBGSF_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(LIBPNG_CFLAGS) \
	$(POPPLER_GLIB_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(XINE_CFLAGS) \
	-DMODULES_DIR=\"$(modulesdir)\"

modules_LTLIBRARIES = \
	libextract-abw.la \
	libextract-imagemagick.la \
	libextract-mp3.la \
	libextract-mplayer.la \
	libextract-oasis.la \
	libextract-png.la \
	libextract-ps.la \
	libextract-totem.la

if HAVE_EXEMPI
modules_LTLIBRARIES += libextract-xmp.la
endif

if HAVE_LIBEXIF
modules_LTLIBRARIES += libextract-exif.la
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

libextract_abw_la_SOURCES = tracker-extract-abw.c
libextract_abw_la_LDFLAGS = $(module_flags)
libextract_abw_la_LIBADD = $(GLIB2_LIBS)

libextract_imagemagick_la_SOURCES = \
	tracker-extract-imagemagick.c	\
	tracker-xmp.c tracker-xmp.h
libextract_imagemagick_la_LDFLAGS = $(module_flags)
libextract_imagemagick_la_INCLUDES = $(GLIB2_CFLAGS) $(EXEMPI_CFLAGS)
libextract_imagemagick_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

libextract_mp3_la_SOURCES = tracker-extract-mp3.c
libextract_mp3_la_LDFLAGS = $(module_flags)
libextract_mp3_la_LIBADD = $(GLIB2_LIBS)

libextract_mplayer_la_SOURCES = tracker-extract-mplayer.c
libextract_mplayer_la_LDFLAGS = $(module_flags)
libextract_mplayer_la_LIBADD = $(GLIB2_LIBS)

libextract_oasis_la_SOURCES = tracker-extract-oasis.c
libextract_oasis_la_LDFLAGS = $(module_flags)
libextract_oasis_la_LIBADD = $(GLIB2_LIBS)

libextract_png_la_SOURCES = \
	tracker-extract-png.c	\
	tracker-xmp.c tracker-xmp.h
libextract_png_la_LDFLAGS = $(module_flags)
libextract_png_la_LIBADD = $(GLIB2_LIBS) $(LIBPNG_LIBS) $(EXEMPI_LIBS)

libextract_ps_la_SOURCES = tracker-extract-ps.c
libextract_ps_la_LDFLAGS = $(module_flags)
libextract_ps_la_LIBADD = $(GLIB2_LIBS)

libextract_totem_la_SOURCES = tracker-extract-totem.c
libextract_totem_la_LDFLAGS = $(module_flags)
libextract_totem_la_LIBADD = $(GLIB2_LIBS)

libextract_xmp_la_SOURCES = \
	tracker-extract-xmp.c	\
	tracker-xmp.c tracker-xmp.h
libextract_xmp_la_LDFLAGS = $(module_flags)
libextract_xmp_la_LIBADD = $(GLIB2_LIBS) $(EXEMPI_LIBS)

libextract_exif_la_SOURCES = tracker-extract-exif.c
libextract_exif_la_LDFLAGS = $(module_flags)
libextract_exif_la_LIBADD = $(GLIB2_LIBS) $(LIBEXIF_LIBS)

libextract_html_la_SOURCES = tracker-extract-html.c
libextract_html_la_LDFLAGS = $(module_flags)
libextract_html_la_LIBADD = $(GLIB2_LIBS) $(LIBXML2_LIBS)

libextract_msoffice_la_SOURCES = tracker-extract-msoffice.c
libextract_msoffice_la_LDFLAGS = $(module_flags)
libextract_msoffice_la_LIBADD = $(GLIB2_LIBS) $(LIBGSF_LIBS)

libextract_pdf_la_SOURCES = \
	tracker-extract-pdf.c	\
	tracker-xmp.c tracker-xmp.h
libextract_pdf_la_LDFLAGS = $(module_flags)
libextract_pdf_la_LIBADD = $(GLIB2_LIBS) $(POPPLER_GLIB_LIBS) $(EXEMPI_LIBS)

libextract_gstreamer_la_SOURCES = tracker-extract-gstreamer.c
libextract_gstreamer_la_LDFLAGS = $(module_flags)
libextract_gstreamer_la_LIBADD = $(GLIB2_LIBS) $(GSTREAMER_LIBS)

libextract_xine_la_SOURCES = tracker-extract-libxine.c
libextract_xine_la_LDFLAGS = $(module_flags)
libextract_xine_la_LIBADD = $(GLIB2_LIBS) $(XINE_LIBS)


bin_PROGRAMS = tracker-extract

tracker_extract_SOURCES = \
	tracker-extract.c			\
	tracker-extract.h

tracker_extract_LDADD = \
	$(GLIB2_LIBS)				\
	$(GTHREAD_LIBS)				\
	$(GMODULE_LIBS)

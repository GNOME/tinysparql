INCLUDES = $(GLIB2_CFLAGS) $(CFLAGS) -g		\
	$(POPPLER_GLIB_CFLAGS)			\
	$(LIBPNG_CFLAGS)			\
	$(LIBEXIF_CFLAGS)			\
	$(LIBGSF_CFLAGS)			\
	$(LIBGSF_CFLAGS)			\
	$(GSTREAMER_CFLAGS)			\
	$(XINE_CFLAGS)

bin_PROGRAMS = tracker-extract

if USING_GSTREAMER
video_sources = tracker-extract-gstreamer.c
else
if USING_LIBXINE
video_sources = tracker-extract-libxine.c
else
if USING_EXTERNAL_VIDEO_PLAYER
video_sources = tracker-extract-mplayer.c	\
	tracker-extract-totem.c
else
video_sources =
endif
endif
endif

tracker_extract_SOURCES = tracker-extract.c	\
	tracker-extract-oasis.c			\
	tracker-extract-ps.c			\
	tracker-extract-pdf.c			\
	tracker-extract-abw.c			\
	tracker-extract-png.c			\
	tracker-extract-exif.c			\
	tracker-extract-msoffice.c		\
	tracker-extract-imagemagick.c		\
	$(video_sources)

tracker_extract_LDADD = $(GLIB2_LIBS)		\
	$(POPPLER_GLIB_LIBS)			\
	$(LIBPNG_LIBS)				\
	$(LIBEXIF_LIBS)				\
	$(LIBGSF_LIBS)				\
	$(GSTREAMER_LIBS)			\
	$(XINE_LIBS)

INCLUDES = $(GLIB2_CFLAGS) $(CFLAGS) -g \
	$(POPPLER_GLIB_CFLAGS)			\
	$(VORBIS_CFLAGS)			\
	$(THEORA_CFLAGS)			\
	$(LIBPNG_CFLAGS)			\
	$(LIBEXIF_CFLAGS)			\
	$(LIBGSF_CFLAGS)

bin_PROGRAMS = tracker-extract

tracker_extract_SOURCES = tracker-extract.c \
	tracker-extract-oasis.c	\
	tracker-extract-ps.c	\
	tracker-extract-pdf.c	\
	tracker-extract-abw.c	\
	tracker-extract-vorbis.c \
	tracker-extract-mp3.c	\
	tracker-extract-png.c \
	tracker-extract-exif.c	\
	tracker-extract-msoffice.c \
	tracker-extract-imagemagick.c \
	tracker-extract-mplayer.c 


tracker_extract_LDADD = $(GLIB2_LIBS)  \
 		 $(POPPLER_GLIB_LIBS) \
 		 $(VORBIS_LIBS) \
 		 $(THEORA_LIBS) \
 		 $(LIBPNG_LIBS) \
 		 $(LIBEXIF_LIBS) \
		 $(LIBGSF_LIBS)

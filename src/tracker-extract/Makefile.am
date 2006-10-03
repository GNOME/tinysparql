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
	tracker-extract-vorbis.c	\
	tracker-extract-png.c \
	tracker-extract-exif.c	\
	tracker-extract-msoffice.c

if USING_INTERNAL_LIBEXTRACTOR
extractor_ldadd = $(top_builddir)/src/libextractor/src/main/libextractor.la
else
extractor_ldadd = -lextractor
endif

tracker_extract_LDADD = $(GLIB2_LIBS) $(extractor_ldadd) \
 		 $(POPPLER_GLIB_LIBS) \
 		 $(VORBIS_LIBS) \
 		 $(THEORA_LIBS) \
 		 $(LIBPNG_LIBS) \
 		 $(LIBEXIF_LIBS) \
		 $(LIBGSF_LIBS)

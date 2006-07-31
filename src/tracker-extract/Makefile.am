INCLUDES = $(GLIB2_CFLAGS) $(CFLAGS) -g 

bin_PROGRAMS = tracker-extract

tracker_extract_SOURCES = tracker-extract.c

if USING_INTERNAL_LIBEXTRACTOR
extractor_ldadd = $(top_builddir)/src/libextractor/src/main/libextractor.la
else
extractor_ldadd = -lextractor
endif

tracker_extract_LDADD = $(GLIB2_LIBS) $(extractor_ldadd)

		

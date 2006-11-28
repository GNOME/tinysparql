INCLUDES = $(GLIB2_CFLAGS) $(CFLAGS) -g		\
	$(LIBPNG_CFLAGS)

bin_PROGRAMS = tracker-thumbnailer

tracker_thumbnailer_SOURCES = tracker-thumbnailer.c	\
	md5.c			\
	md5.h

tracker_thumbnailer_LDADD = $(GLIB2_LIBS)		\
	$(LIBPNG_LIBS)

include $(top_srcdir)/Makefile.decl

INCLUDES = 				\
	$(GLIB2_CFLAGS)			\
	$(LIBPNG_CFLAGS) 		\
	-DLIBDIR=\""$(libdir)"\" 

libexec_PROGRAMS = tracker-thumbnailer

tracker_thumbnailer_SOURCES = 		\
	tracker-thumbnailer.c

tracker_thumbnailer_LDADD = 		\
	$(GLIB2_LIBS) 			\
	$(LIBPNG_LIBS)

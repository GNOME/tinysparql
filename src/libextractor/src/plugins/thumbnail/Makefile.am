include ../Makefile-plugins.am

plugin_LTLIBRARIES = \
 libextractor_thumbnail.la

AM_CFLAGS = $(GLIB_CFLAGS) $(GTK_CFLAGS)

libextractor_thumbnail_la_CFLAGS = \
  $(GLIB_CFLAGS) $(GTK_CFLAGS)
libextractor_thumbnail_la_LIBADD = \
  $(LIBADD) -lgobject-2.0 @GTK_LIBS@ \
  $(top_builddir)/src/main/libextractor.la 
libextractor_thumbnail_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand) 
libextractor_thumbnail_la_SOURCES = \
  thumbnailextractor.c

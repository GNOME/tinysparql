include ../Makefile-plugins.am


plugin_LTLIBRARIES = \
  libextractor_ole2.la

AM_CFLAGS = $(GLIB_CFLAGS)

libextractor_ole2_la_CFLAGS = \
  $(GLIB_CFLAGS) 
libextractor_ole2_la_LIBADD = \
  $(LIBADD) $(GLIB_LIBS) -lgobject-2.0 \
  $(top_builddir)/src/main/libextractor.la
libextractor_ole2_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand) 
libextractor_ole2_la_SOURCES =  \
        ole2extractor.c

include ../Makefile-plugins.am

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_oo.la

libextractor_oo_la_SOURCES = \
  ooextractor.c

libextractor_oo_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_oo_la_LIBADD = \
 $(top_builddir)/src/main/libextractor.la \
 -lz 

include ../Makefile-plugins.am

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_hash_md5.la \
 libextractor_hash_sha1.la \
 libextractor_hash_rmd160.la

libextractor_hash_md5_la_SOURCES = \
  md5extractor.c
libextractor_hash_md5_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_hash_sha1_la_SOURCES = \
  sha1extractor.c
libextractor_hash_sha1_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_hash_rmd160_la_SOURCES = \
  rmd160extractor.c
libextractor_hash_rmd160_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

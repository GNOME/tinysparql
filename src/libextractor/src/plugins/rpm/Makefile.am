include ../Makefile-plugins.am

SUBDIRS = .

if MINGW
  WINFLAGS = -lpthread -lws2_32
endif

plugin_LTLIBRARIES = \
 libextractor_rpm.la

libextractor_rpm_la_SOURCES = \
 rpmextractor.c
libextractor_rpm_la_LDFLAGS = \
 $(PLUGINFLAGS)  $(retaincommand) \
 $(WINFLAGS)

include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-I$(top_srcdir)/src				\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	$(GIO_CFLAGS)					\
	$(PANGO_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(UNAC_CFLAGS)

libtracker_commondir = $(libdir)/tracker
libtracker_commonincludedir=$(includedir)/tracker-1.0/libtracker-common/
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

if HAVE_HAL
hal_sources = 						\
	tracker-hal.c
hal_headers = 						\
	tracker-hal.h
endif

libtracker_common_la_SOURCES =	 			\
	$(os_sources)					\
	$(hal_sources)					\
	tracker-config.c 				\
	tracker-dbus.c	 				\
	tracker-field.c					\
	tracker-file-utils.c				\
	tracker-ioprio.c				\
	tracker-language.c				\
	tracker-log.c	 				\
	tracker-marshal-main.c				\
	tracker-module-config.c				\
	tracker-nfs-lock.c				\
	tracker-ontology.c				\
	tracker-parser.c				\
	tracker-service.c				\
	tracker-type-utils.c				\
	tracker-utils.c					\
	tracker-thumbnailer.c				\
	tracker-albumart.c

noinst_HEADERS =					\
	$(hal_headers)					\
	tracker-dbus.h					\
	tracker-ioprio.h				\
	tracker-log.h					\
	tracker-nfs-lock.h				\
	tracker-os-dependant.h				\
	tracker-thumbnailer.h				\
	tracker-albumart.h

libtracker_commoninclude_HEADERS =			\
	tracker-common.h				\
	tracker-config.h				\
	tracker-field.h					\
	tracker-file-utils.h				\
	tracker-language.h				\
	tracker-module-config.h				\
	tracker-ontology.h				\
	tracker-parser.h				\
	tracker-service.h				\
	tracker-type-utils.h				\
	tracker-utils.h

libtracker_common_la_LDFLAGS = -version-info 0:0:0
libtracker_common_la_LIBADD = 				\
	$(top_builddir)/src/libstemmer/libstemmer.la	\
	$(HAL_LIBS)					\
	$(DBUS_LIBS)					\
	$(UNAC_LIBS)					\
	$(PANGO_LIBS)					\
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(GDKPIXBUF_LIBS)


tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body > $@

tracker-marshal-main.c: tracker-marshal.c tracker-marshal.h

marshal_sources =                              		\
        tracker-marshal.h              			\
        tracker-marshal.c

BUILT_SOURCES =						\
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

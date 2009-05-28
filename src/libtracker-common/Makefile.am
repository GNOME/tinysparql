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

libtracker_commondir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_commonincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-common/
libtracker_common_LTLIBRARIES = libtracker-common.la

if OS_WIN32
os_sources = tracker-os-dependant-win.c
else
os_sources = tracker-os-dependant-unix.c
endif

if HAVE_HAL
hal_sources = 						\
	tracker-power-hal.c				\
	tracker-storage-hal.c
hal_headers = 						\
	tracker-power.h					\
	tracker-storage.h
endif

libtracker_common_la_VALASOURCES =			\
	tracker-sparql-builder.vala

libtracker_common_la_SOURCES =	 			\
	$(os_sources)					\
	$(hal_sources)					\
	tracker-class.c					\
	tracker-config.c 				\
	tracker-dbus.c	 				\
	tracker-file-utils.c				\
	tracker-ioprio.c				\
	tracker-language.c				\
	tracker-log.c	 				\
	tracker-marshal-main.c				\
	tracker-module-config.c				\
	tracker-namespace.c				\
	tracker-nfs-lock.c				\
	tracker-ontology.c				\
	tracker-parser.c				\
	tracker-property.c				\
	tracker-statement-list.c			\
	tracker-status.c				\
	tracker-type-utils.c				\
	tracker-utils.c					\
	tracker-thumbnailer.c				\
	tracker-albumart.c				\
	libtracker-common.vala.stamp			\
	$(libtracker_common_la_VALASOURCES:.vala=.c)

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
	tracker-class.h					\
	tracker-common.h				\
	tracker-config.h				\
	tracker-file-utils.h				\
	tracker-language.h				\
	tracker-module-config.h				\
	tracker-namespace.h				\
	tracker-ontology.h				\
	tracker-parser.h				\
	tracker-property.h				\
	tracker-sparql-builder.h						\
	tracker-statement-list.h			\
	tracker-status.h				\
	tracker-type-utils.h				\
	tracker-utils.h

libtracker-common.vala.stamp: $(libtracker_common_la_VALASOURCES)
	$(VALAC) -C $(VALAFLAGS) -H tracker-sparql-builder.h $^
	touch $@

libtracker_common_la_LDFLAGS = 				\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

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
	$(marshal_sources)				\
	libtracker-common.vala.stamp

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 						\
	tracker-marshal.list				\
	$(libtracker_common_la_VALASOURCES)		\
	libtracker-common.vala.stamp			\
	libtracker-common.vapi


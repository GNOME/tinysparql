include $(top_srcdir)/Makefile.decl

SUBDIRS = modules

INCLUDES =								\
	-Wall								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DPKGLIBDIR=\""$(libdir)/tracker"\"				\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/indexer-modules"\"	\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(RAPTOR_CFLAGS)

libtracker_moduledir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_moduleincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-module/
libtracker_module_LTLIBRARIES = libtracker-module.la

libtracker_module_la_SOURCES =	 					\
	tracker-dbus.c							\
	tracker-module-file.c						\
	tracker-module-iteratable.c					\
	tracker-module-metadata-utils.c

libtracker_moduleinclude_HEADERS =					\
	tracker-module.h						\
	tracker-module-file.h						\
	tracker-module-iteratable.h					\
	tracker-module-metadata-utils.h

libtracker_module_la_LIBADD = $(GCOV_LIBS)

libexec_PROGRAMS = tracker-miner-fs

tracker_miner_fs_SOURCES =						\
	$(marshal_sources)						\
	$(dbus_sources)							\
	tracker-config.c						\
	tracker-config.h						\
	tracker-crawler.c						\
	tracker-crawler.h						\
	tracker-dbus.h							\
	tracker-indexer.c						\
	tracker-indexer.h						\
	tracker-indexer-module.c					\
	tracker-indexer-module.h					\
	tracker-main.c							\
	tracker-miner.c							\
	tracker-miner.h							\
	tracker-monitor.c						\
	tracker-monitor.h						\
	tracker-processor.c						\
	tracker-processor.h						\
	tracker-removable-device.c					\
	tracker-removable-device.h					\
	tracker-status.c						\
	tracker-status.h						\
	tracker-thumbnailer.c						\
	tracker-thumbnailer.h						\
	tracker-utils.c							\
	tracker-utils.h							\
	tracker-volume-cleanup.c					\
	tracker-volume-cleanup.h

tracker_miner_fs_LDADD =						\
	$(plugin_libs)							\
	libtracker-module.la						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(top_builddir)/src/libinotify/libinotify.la			\
	$(tracker_store_win_libs)					\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)							\
	$(RAPTOR_LIBS)							\
	-lz								\
	-lm

marshal_sources =							\
	tracker-marshal.h						\
	tracker-marshal.c

dbus_sources = 								\
	tracker-miner-glue.h						\
	tracker-extract-client.h

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	echo "#include \"tracker-marshal.h\"" > $@ &&			\
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(marshal_sources)						\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

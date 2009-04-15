include $(top_srcdir)/Makefile.decl

SUBDIRS = modules

INCLUDES =								\
	-Wall								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker/indexer-modules"\"	\
	-DPUSH_MODULES_DIR=\""$(libdir)/tracker/push-modules/indexer"\" \
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

libtracker_moduledir = $(libdir)/tracker
libtracker_moduleincludedir=$(includedir)/tracker-1.0/libtracker-module/
libtracker_module_LTLIBRARIES = libtracker-module.la

libtracker_module_la_SOURCES =	 					\
	tracker-dbus.c							\
	tracker-module-file.c						\
	tracker-module-iteratable.c					\
	tracker-module-metadata-utils.c					\
	tracker-module-metadata.c

libtracker_moduleinclude_HEADERS =					\
	tracker-module.h						\
	tracker-module-file.h						\
	tracker-module-iteratable.h					\
	tracker-module-metadata-utils.h					\
	tracker-module-metadata.h

libtracker_module_la_LIBADD = $(GCOV_LIBS)

libexec_PROGRAMS = tracker-indexer

tracker_indexer_SOURCES =						\
	tracker-dbus.h							\
	tracker-indexer.c						\
	tracker-indexer.h						\
	tracker-indexer-module.c					\
	tracker-indexer-module.h					\
	tracker-main.c							\
	tracker-module-metadata-private.h				\
	tracker-marshal-main.c						\
	tracker-removable-device.c					\
	tracker-removable-device.h					\
	tracker-push.c							\
	tracker-push.h

tracker_indexer_LDADD =							\
	$(plugin_libs)							\
	libtracker-module.la						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(trackerd_win_libs)						\
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

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body > $@

tracker-marshal-main.c: tracker-marshal.c tracker-marshal.h

marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

dbus_sources = 								\
	tracker-indexer-glue.h						\
	tracker-extract-client.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

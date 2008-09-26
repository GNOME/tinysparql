include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMAIL_MODULES_DIR=\""$(libdir)"/tracker/mail-modules\"         \
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	$(GMIME_CFLAGS)							\
	$(FAM_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(QDBM_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(PANGO_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

if HAVE_INOTIFY
inotify_libs = $(top_builddir)/src/libinotify/libinotify.la
endif

#
# Daemon sources
#
libexec_PROGRAMS = trackerd

trackerd_SOURCES =							\
	tracker-crawler.c						\
	tracker-crawler.h						\
	tracker-daemon.c						\
	tracker-daemon.h						\
	tracker-db.c							\
	tracker-db.h							\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-files.c							\
	tracker-files.h							\
	tracker-keywords.c						\
	tracker-keywords.h						\
	tracker-processor.c						\
	tracker-processor.h						\
	tracker-main.c							\
	tracker-main.h							\
	tracker-marshal-main.c						\
	tracker-metadata.c						\
	tracker-metadata.h						\
	tracker-monitor.c						\
	tracker-monitor.h						\
	tracker-query-tree.c						\
	tracker-query-tree.h						\
	tracker-rdf-query.c						\
	tracker-rdf-query.h						\
	tracker-search.c						\
	tracker-search.h						\
	tracker-status.c						\
	tracker-status.h						\
	tracker-utils.c							\
	tracker-utils.h							\
	tracker-xesam.c							\
	tracker-xesam.h							\
	tracker-xesam-manager.c						\
	tracker-xesam-manager.h						\
	tracker-xesam-query.c						\
	tracker-xesam-query.h						\
	tracker-xesam-session.c						\
	tracker-xesam-session.h						\
	tracker-xesam-live-search.c					\
	tracker-xesam-live-search.h

if OS_WIN32
trackerd_win_libs = -lws2_32 -lkernel32
endif

trackerd_LDADD =							\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(inotify_libs)							\
	$(GMIME_LIBS)							\
	$(FAM_LIBS)							\
	$(SQLITE3_LIBS)							\
	$(QDBM_LIBS)							\
	$(DBUS_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(trackerd_win_libs)						\
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
	tracker-daemon-glue.h						\
	tracker-files-glue.h						\
	tracker-keywords-glue.h						\
	tracker-metadata-glue.h						\
	tracker-search-glue.h						\
	tracker-xesam-glue.h						\
	tracker-indexer-client.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 								\
	$(BUILT_SOURCES)						\
	tracker-marshal.list


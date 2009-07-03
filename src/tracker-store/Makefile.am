include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMAIL_MODULES_DIR=\""$(libdir)"/tracker-$(TRACKER_API_VERSION)/mail-modules\"         \
	-DPUSH_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/push-modules/daemon"\"	\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GIO_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(PANGO_CFLAGS)							\
	$(GMIME_CFLAGS)							\
	$(FAM_CFLAGS)							\
	$(SQLITE3_CFLAGS)						\
	$(RAPTOR_CFLAGS)

#
# Daemon sources
#
libexec_PROGRAMS = tracker-store

tracker_store_SOURCES =							\
	tracker-backup.h						\
	tracker-backup.c						\
	tracker-daemon.c						\
	tracker-daemon.h						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-events.c						\
	tracker-events.h						\
	tracker-main.c							\
	tracker-main.h							\
	tracker-marshal-main.c						\
	tracker-resources.c						\
	tracker-resources.h						\
	tracker-search.c						\
	tracker-search.h						\
	tracker-volume-cleanup.c					\
	tracker-volume-cleanup.h					\
	tracker-push.c							\
	tracker-push.h							\
	tracker-push-registrar.c					\
	tracker-push-registrar.h					\
	tracker-resource-class.c					\
	tracker-resource-class.h					\
	tracker-store.c							\
	tracker-store.h

if OS_WIN32
tracker_store_win_libs = -lws2_32 -lkernel32
endif

tracker_store_LDADD =							\
	$(plugin_libs)							\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(GMIME_LIBS)							\
	$(SQLITE3_LIBS)							\
	$(DBUS_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(RAPTOR_LIBS)							\
	$(GCOV_LIBS)							\
	$(tracker_store_win_libs)						\
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
	tracker-backup-glue.h						\
	tracker-daemon-glue.h						\
	tracker-resources-glue.h					\
	tracker-search-glue.h						\
	tracker-resources-class-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list


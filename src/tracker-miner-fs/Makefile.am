include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-Wall								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DPKGLIBDIR=\""$(libdir)/tracker"\"				\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GDKPIXBUF_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(RAPTOR_CFLAGS)

libexec_PROGRAMS = tracker-miner-fs-new

tracker_miner_fs_new_SOURCES =						\
	tracker-config.c						\
	tracker-config.h						\
	tracker-miner-applications.c					\
	tracker-miner-applications.h					\
	tracker-miner-files.c						\
	tracker-miner-files.h						\
	tracker-miner-main.c

tracker_miner_fs_new_LDADD =						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(top_builddir)/src/libinotify/libinotify.la			\
	$(top_builddir)/src/libtracker-miner/libtracker-miner.la	\
	$(tracker_store_win_libs)					\
	$(DBUS_LIBS)							\
	$(GDKPIXBUF_LIBS)						\
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
	tracker-miner-glue.h						\
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

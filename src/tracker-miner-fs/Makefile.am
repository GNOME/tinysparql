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
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GCOV_CFLAGS)

libexec_PROGRAMS = tracker-miner-fs

tracker_miner_fs_SOURCES =						\
	tracker-config.c						\
	tracker-config.h						\
	tracker-miner-applications.c					\
	tracker-miner-applications.h					\
	tracker-miner-files.c						\
	tracker-miner-files.h						\
	tracker-miner-main.c

tracker_miner_fs_LDADD =						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(top_builddir)/src/libinotify/libinotify.la			\
	$(top_builddir)/src/libtracker-miner/libtracker-miner.la	\
	$(tracker_store_win_libs)					\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)							\
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

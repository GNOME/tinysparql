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
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(WARN_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(GCOV_CFLAGS)

libexec_PROGRAMS = tracker-miner-fs

tracker_miner_fs_SOURCES =						\
	tracker-config.c						\
	tracker-config.h						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-main.c							\
	tracker-miner-applications.c					\
	tracker-miner-applications.h					\
	tracker-miner-files.c						\
	tracker-miner-files.h						\
	tracker-miner-files-reindex.c					\
	tracker-miner-files-reindex.h

tracker_miner_fs_LDADD =						\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
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
	tracker-miner-files-reindex-glue.h				\
	tracker-extract-client.h

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ &&	\
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(marshal_sources)						\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list
